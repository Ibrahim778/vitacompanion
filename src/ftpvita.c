/*
 * Copyright (c) 2015-2016 Sergi Granell (xerpi)
 */

#include "ftpvita.h"

#include <paf/std/stdio.h>
#include <paf/std/stdlib.h>
#include <paf/std/string.h>
#include <net.h>
#include <libnetctl.h>
#include <rtc.h>

#ifdef _FTPVITA_DEBUG_
#define DEBUG(...) sceClibPrintf(__VA_ARGS__)
#else
#define DEBUG(...) {(void)SCE_NULL;}
#endif

#define UNUSED(x) (void)(x)

#define NET_CTL_ERROR_NOT_TERMINATED 0x80412102

#define FTP_PORT 1337
#define NET_INIT_SIZE (64 * 1024)
#define DEFAULT_FILE_BUF_SIZE (4 * 1024 * 1024)

#define FTP_DEFAULT_PATH   "/"

#define MAX_DEVICES 16
#define MAX_CUSTOM_COMMANDS 16

/* PSVita paths are in the form:
 *     <device name>:<filename in device>
 * for example: cache0:/foo/bar
 * We will send Unix-like paths to the FTP client, like:
 *     /cache0:/foo/bar
 */

typedef struct {
	const char *cmd;
	cmd_dispatch_func func;
} cmd_dispatch_entry;

static struct {
	char name[SCE_IO_MAX_PATH_LENGTH];
	int valid;
} device_list[MAX_DEVICES];

static struct {
	const char *cmd;
	cmd_dispatch_func func;
	int valid;
} custom_command_dispatchers[MAX_CUSTOM_COMMANDS];

static void *net_memory = NULL;
static int ftp_initialized = 0;
static unsigned int file_buf_size = DEFAULT_FILE_BUF_SIZE;
static SceNetInAddr vita_addr;
static SceUID server_thid;
static int server_sockfd;
static int number_clients = 0;
static ftpvita_client_info_t *client_list = NULL;
static SceUID client_list_mtx;

static int netctl_init = -1;
static int net_init = -1;

static void (*info_log_cb)(const char *) = NULL;
static void (*debug_log_cb)(const char *) = NULL;

#define client_send_ctrl_msg(cl, str) \
	sceNetSend(cl->ctrl_sockfd, str, sce_paf_strlen(str), 0)

static inline void client_send_data_msg(ftpvita_client_info_t *client, const char *str)
{
	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		sceNetSend(client->data_sockfd, str, sce_paf_strlen(str), 0);
	} else {
		sceNetSend(client->pasv_sockfd, str, sce_paf_strlen(str), 0);
	}
}

static inline int client_recv_data_raw(ftpvita_client_info_t *client, void *buf, unsigned int len)
{
	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		return sceNetRecv(client->data_sockfd, buf, len, 0);
	} else {
		return sceNetRecv(client->pasv_sockfd, buf, len, 0);
	}
}

static inline void client_send_data_raw(ftpvita_client_info_t *client, const void *buf, unsigned int len)
{
	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		sceNetSend(client->data_sockfd, buf, len, 0);
	} else {
		sceNetSend(client->pasv_sockfd, buf, len, 0);
	}
}

static inline const char *get_vita_path(const char *path)
{
	if (sce_paf_strlen(path) > 1)
		/* /cache0:/foo/bar -> cache0:/foo/bar */
		return &path[1];
	else
		return NULL;
}

static int file_exists(const char *path)
{
	SceIoStat stat;
	return (sceIoGetstat(path, &stat) >= 0);
}

static void cmd_NOOP_func(ftpvita_client_info_t *client)
{
	client_send_ctrl_msg(client, "200 No operation ;)" FTPVITA_EOL);
}

static void cmd_USER_func(ftpvita_client_info_t *client)
{
	client_send_ctrl_msg(client, "331 Username OK, need password b0ss." FTPVITA_EOL);
}

static void cmd_PASS_func(ftpvita_client_info_t *client)
{
	client_send_ctrl_msg(client, "230 User logged in!" FTPVITA_EOL);
}

static void cmd_QUIT_func(ftpvita_client_info_t *client)
{
	client_send_ctrl_msg(client, "221 Goodbye senpai :'(" FTPVITA_EOL);
}

static void cmd_SYST_func(ftpvita_client_info_t *client)
{
	client_send_ctrl_msg(client, "215 UNIX Type: L8" FTPVITA_EOL);
}

static void cmd_PASV_func(ftpvita_client_info_t *client)
{
	int ret;
	UNUSED(ret);

	char cmd[512];
	unsigned int namelen;
	SceNetSockaddrIn picked;

	/* Create data mode socket name */
	char data_socket_name[64];
	sce_paf_snprintf(data_socket_name, sizeof(data_socket_name), "FTPVita_client_%i_data_socket",
		client->num);

	/* Create the data socket */
	client->data_sockfd = sceNetSocket(data_socket_name,
		SCE_NET_AF_INET,
		SCE_NET_SOCK_STREAM,
		0);

	DEBUG("PASV data socket fd: %d\n", client->data_sockfd);

	/* Fill the data socket address */
	client->data_sockaddr.sin_family = SCE_NET_AF_INET;
	client->data_sockaddr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
	/* Let the PSVita choose a port */
	client->data_sockaddr.sin_port = sceNetHtons(0);

	/* Bind the data socket address to the data socket */
	ret = sceNetBind(client->data_sockfd,
		(SceNetSockaddr *)&client->data_sockaddr,
		sizeof(client->data_sockaddr));
	DEBUG("sceNetBind(): 0x%08X\n", ret);

	/* Start listening */
	ret = sceNetListen(client->data_sockfd, 128);
	DEBUG("sceNetListen(): 0x%08X\n", ret);

	/* Get the port that the PSVita has chosen */
	namelen = sizeof(picked);
	sceNetGetsockname(client->data_sockfd, (SceNetSockaddr *)&picked,
		&namelen);

	DEBUG("PASV mode port: 0x%04X\n", picked.sin_port);

	/* Build the command */
	sce_paf_snprintf(cmd, sizeof(cmd), "227 Entering Passive Mode (%hhu,%hhu,%hhu,%hhu,%hhu,%hhu)" FTPVITA_EOL,
		(vita_addr.s_addr >> 0) & 0xFF,
		(vita_addr.s_addr >> 8) & 0xFF,
		(vita_addr.s_addr >> 16) & 0xFF,
		(vita_addr.s_addr >> 24) & 0xFF,
		(picked.sin_port >> 0) & 0xFF,
		(picked.sin_port >> 8) & 0xFF);

	client_send_ctrl_msg(client, cmd);

	/* Set the data connection type to passive! */
	client->data_con_type = FTP_DATA_CONNECTION_PASSIVE;
}

int countChar(const char *str, char c)
{
    int ret = 0;
    for(char *s = str; *s = '\0'; s++)
        if(*s == c)
            ret++;
    return ret;
}

static void cmd_PORT_func(ftpvita_client_info_t *client)
{
	unsigned int data_ip[4];
	unsigned int porthi, portlo;
	unsigned short data_port;
	char ip_str[16];
	SceNetInAddr data_addr;

	/* Using ints because of newlibc's u8 sscanf bug */
	DEBUG("[SCANF_FIXED_UNTESTED] 218: scanf needed on: %s\n", client->recv_cmd_args);
    client_send_ctrl_msg(client, "500 Syntax error, command unrecognized." FTPVITA_EOL);
    if(countChar(client->recv_cmd_args, ',') != 5)
    {
        client_send_ctrl_msg(client, "500 Syntax error, command unrecognized." FTPVITA_EOL);
        return;
    }

    char *previousDelim = client->recv_cmd_args;
    char *currentDelim = sce_paf_strtok(client->recv_cmd_args, ',');
    int i = 0;
    while(currentDelim != SCE_NULL)
    {
        char *currentNum = sce_paf_malloc(currentDelim - previousDelim);
        sce_paf_memset(currentNum, 0, currentDelim - previousDelim);
        sce_paf_strncpy(currentNum, currentDelim, currentDelim - previousDelim - 1);

        if(i < 4)
            data_ip[i] = sce_paf_atoi(currentNum);
        else if(i == 4)
            porthi = sce_paf_atoi(currentNum);
        else    
            portlo = sce_paf_atoi(currentNum);

        sce_paf_free(currentNum);
        previousDelim = currentDelim + 1;
        currentDelim = sce_paf_strtok(client->recv_cmd_args, ',');
    }

    // sscanf(client->recv_cmd_args, "%d,%d,%d,%d,%d,%d",
	// 	&data_ip[0], &data_ip[1], &data_ip[2], &data_ip[3],
	// 	&porthi, &portlo);

	data_port = portlo + porthi*256;

	/* Convert to an X.X.X.X IP string */
	sce_paf_snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
		data_ip[0], data_ip[1], data_ip[2], data_ip[3]);

	/* Convert the IP to a SceNetInAddr */
	sceNetInetPton(SCE_NET_AF_INET, ip_str, &data_addr);

	DEBUG("PORT connection to client's IP: %s Port: %d\n", ip_str, data_port);

	/* Create data mode socket name */
	char data_socket_name[64];
	sce_paf_snprintf(data_socket_name, sizeof(data_socket_name), "FTPVita_client_%i_data_socket",
		client->num);

	/* Create data mode socket */
	client->data_sockfd = sceNetSocket(data_socket_name,
		SCE_NET_AF_INET,
		SCE_NET_SOCK_STREAM,
		0);

	DEBUG("Client %i data socket fd: %d\n", client->num,
		client->data_sockfd);

	/* Prepare socket address for the data connection */
	client->data_sockaddr.sin_family = SCE_NET_AF_INET;
	client->data_sockaddr.sin_addr = data_addr;
	client->data_sockaddr.sin_port = sceNetHtons(data_port);

	/* Set the data connection type to active! */
	client->data_con_type = FTP_DATA_CONNECTION_ACTIVE;

	client_send_ctrl_msg(client, "200 PORT command successful!" FTPVITA_EOL);
}

static void client_open_data_connection(ftpvita_client_info_t *client)
{
	int ret;
	UNUSED(ret);

	unsigned int addrlen;

	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		/* Connect to the client using the data socket */
		ret = sceNetConnect(client->data_sockfd,
			(SceNetSockaddr *)&client->data_sockaddr,
			sizeof(client->data_sockaddr));

		DEBUG("sceNetConnect(): 0x%08X\n", ret);
	} else {
		/* Listen to the client using the data socket */
		addrlen = sizeof(client->pasv_sockaddr);
		client->pasv_sockfd = sceNetAccept(client->data_sockfd,
			(SceNetSockaddr *)&client->pasv_sockaddr,
			&addrlen);
		DEBUG("PASV client fd: 0x%08X\n", client->pasv_sockfd);
	}
}

static void client_close_data_connection(ftpvita_client_info_t *client)
{
	sceNetSocketClose(client->data_sockfd);
	/* In passive mode we have to close the client pasv socket too */
	if (client->data_con_type == FTP_DATA_CONNECTION_PASSIVE) {
		sceNetSocketClose(client->pasv_sockfd);
	}
	client->data_con_type = FTP_DATA_CONNECTION_NONE;
}

static int gen_list_format(char *out, int n, int dir, const SceIoStat *stat, const char *filename)
{
	static const char num_to_month[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	char yt[6];
	SceDateTime cdt;
	sceRtcGetCurrentClockLocalTime(&cdt);

	if  (cdt.year == stat->st_mtime.year) {
		sce_paf_snprintf(yt, sizeof(yt), "%02d:%02d", stat->st_mtime.hour, stat->st_mtime.minute);
	}
	else {
		sce_paf_snprintf(yt, sizeof(yt), "%04d", stat->st_mtime.year);
	}

	return sce_paf_snprintf(out, n,
		"%c%s 1 vita vita %u %s %-2d %s %s" FTPVITA_EOL,
		dir ? 'd' : '-',
		dir ? "rwxr-xr-x" : "rw-r--r--",
		(unsigned int) stat->st_size,
		num_to_month[stat->st_mtime.month<=0?0:(stat->st_mtime.month-1)%12],
		stat->st_mtime.day,
		yt,
		filename);
}

static void send_LIST(ftpvita_client_info_t *client, const char *path)
{
	int i;
	char buffer[512];
	SceUID dir;
	SceIoDirent dirent;
	SceIoStat stat;
	char *devname;
	int send_devices = 0;

	/* "/" path is a special case, if we are here we have
	 * to send the list of devices (aka mountpoints). */
	if (sce_paf_strcmp(path, "/") == 0) {
		send_devices = 1;
	}

	if (!send_devices) {
		dir = sceIoDopen(get_vita_path(path));
		if (dir < 0) {
			client_send_ctrl_msg(client, "550 Invalid directory." FTPVITA_EOL);
			return;
		}
	}

	client_send_ctrl_msg(client, "150 Opening ASCII mode data transfer for LIST." FTPVITA_EOL);

	client_open_data_connection(client);

	if (send_devices) {
		for (i = 0; i < MAX_DEVICES; i++) {
			if (device_list[i].valid) {
				devname = device_list[i].name;
				if (sceIoGetstat(devname, &stat) >= 0) {
					gen_list_format(buffer, sizeof(buffer),	1, &stat, devname);
					client_send_data_msg(client, buffer);
				}
			}
		}
	} else {
		memset(&dirent, 0, sizeof(dirent));

		while (sceIoDread(dir, &dirent) > 0) {
			gen_list_format(buffer, sizeof(buffer), SCE_STM_ISDIR(dirent.d_stat.st_mode),
				&dirent.d_stat, dirent.d_name);
			client_send_data_msg(client, buffer);
			memset(&dirent, 0, sizeof(dirent));
			memset(buffer, 0, sizeof(buffer));
		}

		sceIoDclose(dir);
	}

	// DEBUG("Done sending LIST\n");

	client_close_data_connection(client);
	client_send_ctrl_msg(client, "226 Transfer complete." FTPVITA_EOL);
}

int cpyTillNewLine(char *dest, const char *src, size_t max)
{
    sce_paf_strncpy(dest, src, max);
    for(char *c = dest; *c != '\0'; c++)
        if(*c == '\n' || *c == '\r')
        {
            *c = '\0';
            return 1;
        }
    return -1;
}

static void cmd_LIST_func(ftpvita_client_info_t *client)
{
	char list_path[SCE_IO_MAX_PATH_LENGTH];
    sce_paf_memset(list_path, 0, sizeof(list_path));
	int list_cur_path = 1;

	int n = -1;
    // DEBUG("[SCANF_FIXED] 386: scanf on %s\n", client->recv_cmd_args);
    n = cpyTillNewLine(list_path, client->recv_cmd_args, sizeof(list_path - 1));
    // DEBUG("Path: %s\n", list_path);
    //sscanf(client->recv_cmd_args, "%[^\r\n\t]", list_path);

	if (n > 0 && file_exists(get_vita_path(list_path)))
		list_cur_path = 0;

	if (list_cur_path)
		send_LIST(client, client->cur_path);
	else
		send_LIST(client, list_path);
}

static void cmd_PWD_func(ftpvita_client_info_t *client)
{
	char msg[SCE_IO_MAX_PATH_LENGTH];
	sce_paf_snprintf(msg, sizeof(msg), "257 \"%s\" is the current directory." FTPVITA_EOL, client->cur_path);
	client_send_ctrl_msg(client, msg);
}

static int path_is_at_root(const char *path)
{
	return _strrchr(path, '/') == (path + sce_paf_strlen(path) - 1);
}

static void dir_up(char *path)
{
	char *pch;
	size_t len_in = sce_paf_strlen(path);
	if (len_in == 1) {
		sce_paf_strcpy(path, "/");
		return;
	}
	if (path_is_at_root(path)) { /* Case root of the device (/foo0:/) */
		sce_paf_strcpy(path, "/");
	} else {
		pch = _strrchr(path, '/');
		size_t s = len_in - (pch - path);
		sce_paf_memset(pch, '\0', s);
		/* If the path is like: /foo: add slash */
		if (_strrchr(path, '/') == path)
			sce_paf_strcat(path, "/");
	}
}

static void cmd_CWD_func(ftpvita_client_info_t *client)
{
	char cmd_path[SCE_IO_MAX_PATH_LENGTH];
	char tmp_path[SCE_IO_MAX_PATH_LENGTH];
	SceUID pd;
    // DEBUG("[SCANF_FIXED] 435: scanf on %s\n", client->recv_cmd_args);
    int n = cpyTillNewLine(cmd_path, client->recv_cmd_args, sizeof(cmd_path) - 1);
	//sscanf(client->recv_cmd_args, "%[^\r\n\t]", cmd_path);

	if (n < 1) {
		client_send_ctrl_msg(client, "500 Syntax error, command unrecognized." FTPVITA_EOL);
	} else {
		if (sce_paf_strcmp(cmd_path, "/") == 0) {
			sce_paf_strcpy(client->cur_path, cmd_path);
		} else  if (sce_paf_strcmp(cmd_path, "..") == 0) {
			dir_up(client->cur_path);
		} else {
			if (cmd_path[0] == '/') { /* Full path */
				sce_paf_strcpy(tmp_path, cmd_path);
			} else { /* Change dir relative to current dir */
				/* If we are at the root of the device, don't add
				 * an slash to add new path */
				if (path_is_at_root(client->cur_path))
					sce_paf_snprintf(tmp_path, sizeof(tmp_path), "%s%s", client->cur_path, cmd_path);
				else
					sce_paf_snprintf(tmp_path, sizeof(tmp_path), "%s/%s", client->cur_path, cmd_path);
			}

			/* If the path is like: /foo: add an slash */
			if (_strrchr(tmp_path, '/') == tmp_path)
				sce_paf_strcat(tmp_path, "/");

			/* If the path is not "/", check if it exists */
			if (sce_paf_strcmp(tmp_path, "/") != 0) {
				/* Check if the path exists */
				pd = sceIoDopen(get_vita_path(tmp_path));
				if (pd < 0) {
					client_send_ctrl_msg(client, "550 Invalid directory." FTPVITA_EOL);
					return;
				}
				sceIoDclose(pd);
			}
			sce_paf_strcpy(client->cur_path, tmp_path);
		}
		client_send_ctrl_msg(client, "250 Requested file action okay, completed." FTPVITA_EOL);
	}
}

static void cmd_TYPE_func(ftpvita_client_info_t *client)
{
	char data_type = client->recv_cmd_args[0];
	// char format_control[8];
    // DEBUG("[SCANF_FIXED] 481: scanf on %s\n", client->recv_cmd_args);
	int n_args = 1; // sscanf(client->recv_cmd_args, "%c %s", &data_type, format_control);

	if (n_args > 0) {
		switch(data_type) {
		case 'A':
		case 'I':
			client_send_ctrl_msg(client, "200 Okay" FTPVITA_EOL);
			break;
		case 'E':
		case 'L':
		default:
			client_send_ctrl_msg(client, "504 Error: bad parameters?" FTPVITA_EOL);
			break;
		}
	} else {
		client_send_ctrl_msg(client, "504 Error: bad parameters?" FTPVITA_EOL);
	}
}

static void cmd_CDUP_func(ftpvita_client_info_t *client)
{
	dir_up(client->cur_path);
	client_send_ctrl_msg(client, "200 Command okay." FTPVITA_EOL);
}

static void send_file(ftpvita_client_info_t *client, const char *path)
{
	unsigned char *buffer;
	SceUID fd;
	unsigned int bytes_read;

	DEBUG("Opening: %s\n", path);

	if ((fd = sceIoOpen(path, SCE_O_RDONLY, 0777)) >= 0) {

		sceIoLseek32(fd, client->restore_point, SCE_SEEK_SET);

		buffer = sce_paf_malloc(file_buf_size);
		if (buffer == NULL) {
			client_send_ctrl_msg(client, "550 Could not allocate memory." FTPVITA_EOL);
			return;
		}

		client_open_data_connection(client);
		client_send_ctrl_msg(client, "150 Opening Image mode data transfer." FTPVITA_EOL);

		while ((bytes_read = sceIoRead (fd, buffer, file_buf_size)) > 0) {
			client_send_data_raw(client, buffer, bytes_read);
		}

		sceIoClose(fd);
		sce_paf_free(buffer);
		client->restore_point = 0;
		client_send_ctrl_msg(client, "226 Transfer completed." FTPVITA_EOL);
		client_close_data_connection(client);

	} else {
		client_send_ctrl_msg(client, "550 File not found." FTPVITA_EOL);
	}
}

/* This function generates an FTP full-path with the input path (relative or absolute)
 * from RETR, STOR, DELE, RMD, MKD, RNFR and RNTO commands */
static void gen_ftp_fullpath(ftpvita_client_info_t *client, char *path, size_t path_size)
{
	char cmd_path[SCE_IO_MAX_PATH_LENGTH];
	// sscanf(client->recv_cmd_args, "%[^\r\n\t]", cmd_path);
    // DEBUG("[SCANF_FIXED] 549: scanf on %s\n", client->recv_cmd_args);
    cpyTillNewLine(cmd_path, client->recv_cmd_args, sizeof(cmd_path) - 1);
	if (cmd_path[0] == '/') {
		/* Full path */
		sce_paf_strncpy(path, cmd_path, path_size);
	} else {
		if (sce_paf_strlen(cmd_path) >= 5 && cmd_path[3] == ':' && cmd_path[4] == '/') {
			/* Case "ux0:/foo */
			sce_paf_snprintf(path, path_size, "/%s", cmd_path);
		} else {
			/* The file is relative to current dir, so
			 * append the file to the current path */
			sce_paf_snprintf(path, path_size, "%s/%s", client->cur_path, cmd_path);
		}
	}
}

static void cmd_RETR_func(ftpvita_client_info_t *client)
{
	char dest_path[SCE_IO_MAX_PATH_LENGTH];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	send_file(client, get_vita_path(dest_path));
}

static void receive_file(ftpvita_client_info_t *client, const char *path)
{
	unsigned char *buffer;
	SceUID fd;
	int bytes_recv;

	DEBUG("Opening: %s\n", path);

	int mode = SCE_O_CREAT | SCE_O_RDWR;
	/* if we resume broken - append missing part
	 * else - overwrite file */
	if (client->restore_point) {
		mode = mode | SCE_O_APPEND;
	}
	else {
		mode = mode | SCE_O_TRUNC;
	}

	if ((fd = sceIoOpen(path, mode, 0777)) >= 0) {

		buffer = sce_paf_malloc(file_buf_size);
		if (buffer == NULL) {
			client_send_ctrl_msg(client, "550 Could not allocate memory." FTPVITA_EOL);
			return;
		}

		client_open_data_connection(client);
		client_send_ctrl_msg(client, "150 Opening Image mode data transfer." FTPVITA_EOL);

		while ((bytes_recv = client_recv_data_raw(client, buffer, file_buf_size)) > 0) {
			sceIoWrite(fd, buffer, bytes_recv);
		}

		sceIoClose(fd);
		sce_paf_free(buffer);
		client->restore_point = 0;
		if (bytes_recv == 0) {
			client_send_ctrl_msg(client, "226 Transfer completed." FTPVITA_EOL);
		} else {
			sceIoRemove(path);
			client_send_ctrl_msg(client, "426 Connection closed; transfer aborted." FTPVITA_EOL);
		}
		client_close_data_connection(client);

	} else {
		client_send_ctrl_msg(client, "550 File not found." FTPVITA_EOL);
	}
}

static void cmd_STOR_func(ftpvita_client_info_t *client)
{
	char dest_path[SCE_IO_MAX_PATH_LENGTH];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	receive_file(client, get_vita_path(dest_path));
}

static void delete_file(ftpvita_client_info_t *client, const char *path)
{
	DEBUG("Deleting: %s\n", path);

	if (sceIoRemove(path) >= 0) {
		client_send_ctrl_msg(client, "226 File deleted." FTPVITA_EOL);
	} else {
		client_send_ctrl_msg(client, "550 Could not delete the file." FTPVITA_EOL);
	}
}

static void cmd_DELE_func(ftpvita_client_info_t *client)
{
	char dest_path[SCE_IO_MAX_PATH_LENGTH];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	delete_file(client, get_vita_path(dest_path));
}

static void delete_dir(ftpvita_client_info_t *client, const char *path)
{
	int ret;
	DEBUG("Deleting: %s\n", path);
	ret = sceIoRmdir(path);
	if (ret >= 0) {
		client_send_ctrl_msg(client, "226 Directory deleted." FTPVITA_EOL);
	} else if (ret == 0x8001005A) { /* DIRECTORY_IS_NOT_EMPTY */
		client_send_ctrl_msg(client, "550 Directory is not empty." FTPVITA_EOL);
	} else {
		client_send_ctrl_msg(client, "550 Could not delete the directory." FTPVITA_EOL);
	}
}

static void cmd_RMD_func(ftpvita_client_info_t *client)
{
	char dest_path[SCE_IO_MAX_PATH_LENGTH];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	delete_dir(client, get_vita_path(dest_path));
}

static void create_dir(ftpvita_client_info_t *client, const char *path)
{
	DEBUG("Creating: %s\n", path);

	if (sceIoMkdir(path, 0777) >= 0) {
		client_send_ctrl_msg(client, "226 Directory created." FTPVITA_EOL);
	} else {
		client_send_ctrl_msg(client, "550 Could not create the directory." FTPVITA_EOL);
	}
}

static void cmd_MKD_func(ftpvita_client_info_t *client)
{
	char dest_path[SCE_IO_MAX_PATH_LENGTH];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	create_dir(client, get_vita_path(dest_path));
}

static void cmd_RNFR_func(ftpvita_client_info_t *client)
{
	char path_src[SCE_IO_MAX_PATH_LENGTH];
	const char *vita_path_src;
	/* Get the origin filename */
	gen_ftp_fullpath(client, path_src, sizeof(path_src));
	vita_path_src = get_vita_path(path_src);

	/* Check if the file exists */
	if (!file_exists(vita_path_src)) {
		client_send_ctrl_msg(client, "550 The file doesn't exist." FTPVITA_EOL);
		return;
	}
	/* The file to be renamed is the received path */
	sce_paf_strcpy(client->rename_path, vita_path_src);
	client_send_ctrl_msg(client, "350 I need the destination name b0ss." FTPVITA_EOL);
}

static void cmd_RNTO_func(ftpvita_client_info_t *client)
{
	char path_dst[SCE_IO_MAX_PATH_LENGTH];
	const char *vita_path_dst;
	/* Get the destination filename */
	gen_ftp_fullpath(client, path_dst,sizeof(path_dst));
	vita_path_dst = get_vita_path(path_dst);

	DEBUG("Renaming: %s to %s\n", client->rename_path, vita_path_dst);

	if (sceIoRename(client->rename_path, vita_path_dst) < 0) {
		client_send_ctrl_msg(client, "550 Error renaming the file." FTPVITA_EOL);
	}

	client_send_ctrl_msg(client, "226 Rename completed." FTPVITA_EOL);
}

static void cmd_SIZE_func(ftpvita_client_info_t *client)
{
	SceIoStat stat;
	char path[SCE_IO_MAX_PATH_LENGTH];
	char cmd[64];
	/* Get the filename to retrieve its size */
	gen_ftp_fullpath(client, path, sizeof(path));

	/* Check if the file exists */
	if (sceIoGetstat(get_vita_path(path), &stat) < 0) {
		client_send_ctrl_msg(client, "550 The file doesn't exist." FTPVITA_EOL);
		return;
	}
	/* Send the size of the file */
	sce_paf_snprintf(cmd, sizeof(cmd), "213 %lld" FTPVITA_EOL, stat.st_size);
	client_send_ctrl_msg(client, cmd);
}

static int getNum(const char *str, size_t len)
{
    int numBegin = 0;
    for (numBegin = 0; numBegin < len; numBegin++)
    {
        if (str[numBegin] == ' ')
	        break;
    }

    numBegin++;
    int numEnd = numBegin;
    for(;numEnd < len; numEnd++)
    {
        if(!(str[numEnd] > 47 && str[numEnd] < 58)) //Is not a number
            break;
    }
    numEnd--;
    int numStrSize = numEnd - numBegin + 1;
    char *numBuff = sce_paf_malloc(numStrSize + 1);
    sce_paf_memset(numBuff, 0, numStrSize + 1);
    
    sce_paf_strncpy(numBuff, &str[numBegin], numStrSize);
    
    numBegin = sce_paf_atoi(numBuff);
    sce_paf_free(numBuff);
    return numBegin;
}

static void cmd_REST_func(ftpvita_client_info_t *client)
{
	char cmd[64];
    client->restore_point = getNum(client->recv_buffer, sce_paf_strlen(client->recv_buffer));
	// sscanf(client->recv_buffer, "%*[^ ] %d", &client->restore_point); Replaced by function above
	sce_paf_snprintf(cmd, sizeof(cmd), "350 Resuming at %d" FTPVITA_EOL, client->restore_point);
	client_send_ctrl_msg(client, cmd);
}

static void cmd_FEAT_func(ftpvita_client_info_t *client)
{
	/*So client would know that we support resume */
	client_send_ctrl_msg(client, "211-extensions" FTPVITA_EOL);
	client_send_ctrl_msg(client, " REST STREAM" FTPVITA_EOL);
	client_send_ctrl_msg(client, " UTF8" FTPVITA_EOL);
	client_send_ctrl_msg(client, "211 end" FTPVITA_EOL);
}

static void cmd_OPTS_func(ftpvita_client_info_t *client)
{
	client_send_ctrl_msg(client, "501 bad OPTS" FTPVITA_EOL);
}

static void cmd_APPE_func(ftpvita_client_info_t *client)
{
	/* set restore point to not 0
	restore point numeric value only matters if we RETR file from vita.
	If we STOR or APPE, it is only used to indicate that we want to resume
	a broken transfer */
	client->restore_point = -1;
	char dest_path[SCE_IO_MAX_PATH_LENGTH];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	receive_file(client, get_vita_path(dest_path));
}

#define add_entry(name) {#name, cmd_##name##_func}
static const cmd_dispatch_entry cmd_dispatch_table[] = {
	add_entry(NOOP),
	add_entry(USER),
	add_entry(PASS),
	add_entry(QUIT),
	add_entry(SYST),
	add_entry(PASV),
	add_entry(PORT),
	add_entry(LIST),
	add_entry(PWD),
	add_entry(CWD),
	add_entry(TYPE),
	add_entry(CDUP),
	add_entry(RETR),
	add_entry(STOR),
	add_entry(DELE),
	add_entry(RMD),
	add_entry(MKD),
	add_entry(RNFR),
	add_entry(RNTO),
	add_entry(SIZE),
	add_entry(REST),
	add_entry(FEAT),
	add_entry(OPTS),
	add_entry(APPE),
	{NULL, NULL}
};

static cmd_dispatch_func get_dispatch_func(const char *cmd)
{
	int i;
	for(i = 0; cmd_dispatch_table[i].cmd && cmd_dispatch_table[i].func; i++) {
		if (sce_paf_strcmp(cmd, cmd_dispatch_table[i].cmd) == 0) {
			return cmd_dispatch_table[i].func;
		}
	}
	// Check for custom commands
	for(i = 0; i < MAX_CUSTOM_COMMANDS; i++) {
		if (custom_command_dispatchers[i].valid) {
			if (sce_paf_strcmp(cmd, custom_command_dispatchers[i].cmd) == 0) {
				return custom_command_dispatchers[i].func;
			}
		}
	}
	return SCE_NULL;
}

static void client_list_add(ftpvita_client_info_t *client)
{
	/* Add the client at the front of the client list */
	sceKernelLockMutex(client_list_mtx, 1, NULL);

	if (client_list == NULL) { /* List is empty */
		client_list = client;
		client->prev = NULL;
		client->next = NULL;
	} else {
		client->next = client_list;
		client_list->prev = client;
		client->prev = NULL;
		client_list = client;
	}
	client->restore_point = 0;
	number_clients++;

	sceKernelUnlockMutex(client_list_mtx, 1);
}

static void client_list_delete(ftpvita_client_info_t *client)
{
	/* Remove the client from the client list */
	sceKernelLockMutex(client_list_mtx, 1, NULL);

	if (client->prev) {
		client->prev->next = client->next;
	}
	if (client->next) {
		client->next->prev = client->prev;
	}
	if (client == client_list) {
		client_list = client->next;
	}

	number_clients--;

	sceKernelUnlockMutex(client_list_mtx, 1);
}

static void client_list_thread_end()
{
	ftpvita_client_info_t *it, *next;
	SceUID client_thid;
	const int data_abort_flags = SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION |
				SCE_NET_SOCKET_ABORT_FLAG_SND_PRESERVATION;

	sceKernelLockMutex(client_list_mtx, 1, NULL);

	it = client_list;

	/* Iterate over the client list and close their sockets */
	while (it) {
		next = it->next;
		client_thid = it->thid;

		/* Abort the client's control socket, only abort
		 * receiving data so we can still send control messages */
		sceNetSocketAbort(it->ctrl_sockfd,
			SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);

		/* If there's an open data connection, abort it */
		if (it->data_con_type != FTP_DATA_CONNECTION_NONE) {
			sceNetSocketAbort(it->data_sockfd, data_abort_flags);
			if (it->data_con_type == FTP_DATA_CONNECTION_PASSIVE) {
				sceNetSocketAbort(it->pasv_sockfd, data_abort_flags);
			}
		}

		/* Wait until the client threads ends */
		sceKernelWaitThreadEnd(client_thid, NULL, NULL);

		it = next;
	}

	sceKernelUnlockMutex(client_list_mtx, 1);
}

const char *_strrchr(const char *str, char c)
{
    size_t strl = sce_paf_strlen(str);
    for(int i = strl; i > 0; i--)
    {
        if(str[i] == c)
            return &str[i];
    }
    return str;
}

static int client_thread(SceSize args, void *argp)
{
	char cmd[16];

	cmd_dispatch_func dispatch_func;
	ftpvita_client_info_t *client = *(ftpvita_client_info_t **)argp;

	DEBUG("Client thread %i started!\n", client->num);

	client_send_ctrl_msg(client, "220 FTPVita Server ready." FTPVITA_EOL);

	while (1) {
        sce_paf_memset(cmd, 0, sizeof(cmd));
		sce_paf_memset(client->recv_buffer, 0, sizeof(client->recv_buffer));

		client->n_recv = sceNetRecv(client->ctrl_sockfd, client->recv_buffer, sizeof(client->recv_buffer), 0);
		if (client->n_recv > 0) {
			DEBUG("Received %i bytes from client number %i:\n",
				client->n_recv, client->num);

			DEBUG("\t%i> %s", client->num, client->recv_buffer);

			/* The command is the first chars until the first space */
			// sscanf(client->recv_buffer, "%s", cmd);
            sce_paf_strncpy(cmd, client->recv_buffer, 15);
            for(char *c = cmd; *c != '\0'; c++)
                if(*c == ' ' || *c == '\n' || *c == '\r')
                    *c = '\0';
            
            DEBUG("cmd: %s\n", cmd);

            client->recv_cmd_args = sce_paf_strchr(client->recv_buffer, ' ');
            DEBUG("[LOG] client args: %s\n", client->recv_cmd_args ? client->recv_cmd_args + 1 : "None");
			if (client->recv_cmd_args)
				client->recv_cmd_args++; /* Skip the space */
			else
				client->recv_cmd_args = client->recv_buffer;

			/* Wait 1 ms before sending any data */
			sceKernelDelayThread(1*1000);

			if ((dispatch_func = get_dispatch_func(cmd))) {
				dispatch_func(client);
			} else {
				client_send_ctrl_msg(client, "502 Sorry, command not implemented. :(" FTPVITA_EOL);
			}

		} else if (client->n_recv == 0) {
			/* Value 0 means connection closed by the remote peer */
			DEBUG("Connection closed by the client %i.\n", client->num);
			/* Delete itself from the client list */
			client_list_delete(client);
			break;
		} else if (client->n_recv == SCE_NET_ERROR_EINTR) {
			/* Socket aborted (ftpvita_fini() called) */
			DEBUG("Client %i socket aborted.\n", client->num);
			break;
		} else {
			/* Other errors */
			DEBUG("Client %i socket error: 0x%08X\n", client->num, client->n_recv);
			client_list_delete(client);
			break;
		}
	}

	/* Close the client's socket */
	sceNetSocketClose(client->ctrl_sockfd);

	/* If there's an open data connection, close it */
	if (client->data_con_type != FTP_DATA_CONNECTION_NONE) {
		sceNetSocketClose(client->data_sockfd);
		if (client->data_con_type == FTP_DATA_CONNECTION_PASSIVE) {
			sceNetSocketClose(client->pasv_sockfd);
		}
	}

	DEBUG("Client thread %i exiting!\n", client->num);

	sce_paf_free(client);

	sceKernelExitDeleteThread(0);
	return 0;
}

static int server_thread(SceSize args, void *argp)
{
	int ret;
	UNUSED(ret);

	SceNetSockaddrIn serveraddr;

	DEBUG("Server thread started!\n");

	/* Create server socket */
	server_sockfd = sceNetSocket("FTPVita_server_sock",
		SCE_NET_AF_INET,
		SCE_NET_SOCK_STREAM,
		0);

	DEBUG("Server socket fd: %d\n", server_sockfd);

	/* Fill the server's address */
	serveraddr.sin_family = SCE_NET_AF_INET;
	serveraddr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
	serveraddr.sin_port = sceNetHtons(FTP_PORT);

	/* Bind the server's address to the socket */
	ret = sceNetBind(server_sockfd, (SceNetSockaddr *)&serveraddr, sizeof(serveraddr));
	DEBUG("sceNetBind(): 0x%08X\n", ret);

	/* Start listening */
	ret = sceNetListen(server_sockfd, 128);
	DEBUG("sceNetListen(): 0x%08X\n", ret);

	while (1) {
		/* Accept clients */
		SceNetSockaddrIn clientaddr;
		int client_sockfd;
		unsigned int addrlen = sizeof(clientaddr);

		DEBUG("Waiting for incoming connections...\n");

		client_sockfd = sceNetAccept(server_sockfd, (SceNetSockaddr *)&clientaddr, &addrlen);
		if (client_sockfd >= 0) {
			DEBUG("New connection, client fd: 0x%08X\n", client_sockfd);

			/* Get the client's IP address */
			char remote_ip[16];
			sceNetInetNtop(SCE_NET_AF_INET,
				&clientaddr.sin_addr.s_addr,
				remote_ip,
				sizeof(remote_ip));

			DEBUG("Client %i connected, IP: %s port: %i\n",
				number_clients, remote_ip, clientaddr.sin_port);

			/* Create a new thread for the client */
			char client_thread_name[64];
			sce_paf_snprintf(client_thread_name, sizeof(client_thread_name), "FTPVita_client_%i_thread",
				number_clients);

			SceUID client_thid = sceKernelCreateThread(
				client_thread_name, client_thread,
				0x10000100, 0x10000, 0, 0, NULL);

			DEBUG("Client %i thread UID: 0x%08X\n", number_clients, client_thid);

			/* Allocate the ftpvita_client_info_t struct for the new client */
			ftpvita_client_info_t *client = sce_paf_malloc(sizeof(*client));
			client->num = number_clients;
			client->thid = client_thid;
			client->ctrl_sockfd = client_sockfd;
			client->data_con_type = FTP_DATA_CONNECTION_NONE;
			sce_paf_strcpy(client->cur_path, FTP_DEFAULT_PATH);
			memcpy(&client->addr, &clientaddr, sizeof(client->addr));

			/* Add the new client to the client list */
			client_list_add(client);

			/* Start the client thread */
			sceKernelStartThread(client_thid, sizeof(client), &client);
		} else {
			/* if sceNetAccept returns < 0, it means that the listening
			 * socket has been closed, this means that we want to
			 * finish the server thread */
			DEBUG("Server socket closed, 0x%08X\n", client_sockfd);
			break;
		}
	}

	DEBUG("Server thread exiting!\n");

	sceKernelExitDeleteThread(0);
	return 0;
}

int ftpvita_init(char *vita_ip, unsigned short int *vita_port)
{
	int ret;
	int i;
	SceNetInitParam initparam;
	SceNetCtlInfo info;

	if (ftp_initialized) {
		return -1;
	}

	/* Init Net */
	ret = sceNetShowNetstat();
	if (ret == 0) {
		DEBUG("Net is already initialized.\n");
		net_init = -1;
	} else if (ret == SCE_NET_ERROR_ENOTINIT) {
		net_memory = sce_paf_malloc(NET_INIT_SIZE);

		initparam.memory = net_memory;
		initparam.size = NET_INIT_SIZE;
		initparam.flags = 0;

		ret = net_init = sceNetInit(&initparam);
		DEBUG("sceNetInit(): 0x%08X\n", net_init);
		if (net_init < 0)
			goto error_netinit;
	} else {
		DEBUG("Net error: 0x%08X\n", net_init);
		goto error_netstat;
	}

	/* Init NetCtl */
	ret = netctl_init = sceNetCtlInit();
	DEBUG("sceNetCtlInit(): 0x%08X\n", netctl_init);
	if (netctl_init < 0 && netctl_init != NET_CTL_ERROR_NOT_TERMINATED)
		goto error_netctlinit;

	/* Get IP address */
	ret = sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &info);
	DEBUG("sceNetCtlInetGetInfo(): 0x%08X\n", ret);
	if (ret < 0)
		goto error_netctlgetinfo;

	/* Return data */
	sce_paf_strcpy(vita_ip, info.ip_address);
	*vita_port = FTP_PORT;

	/* Save the IP of PSVita to a global variable */
	sceNetInetPton(SCE_NET_AF_INET, info.ip_address, &vita_addr);

	/* Create server thread */
	server_thid = sceKernelCreateThread("FTPVita_server_thread",
		server_thread, 0x10000100, 0x10000, 0, 0, NULL);
	DEBUG("Server thread UID: 0x%08X\n", server_thid);

	/* Create the client list mutex */
	client_list_mtx = sceKernelCreateMutex("FTPVita_client_list_mutex", 0, 0, NULL);
	DEBUG("Client list mutex UID: 0x%08X\n", client_list_mtx);

	/* Init device list */
	for (i = 0; i < MAX_DEVICES; i++) {
		device_list[i].valid = 0;
	}

	for (i = 0; i < MAX_CUSTOM_COMMANDS; i++) {
		custom_command_dispatchers[i].valid = 0;
	}

	/* Start the server thread */
	sceKernelStartThread(server_thid, 0, NULL);

	ftp_initialized = 1;

	return 0;

error_netctlgetinfo:
	if (netctl_init == 0) {
		sceNetCtlTerm();
		netctl_init = -1;
	}
error_netctlinit:
	if (net_init == 0) {
		sceNetTerm();
		net_init = -1;
	}
error_netinit:
	if (net_memory) {
		sce_paf_free(net_memory);
		net_memory = NULL;
	}
error_netstat:
	return ret;
}

void ftpvita_fini()
{
	if (ftp_initialized) {
		/* In order to "stop" the blocking sceNetAccept,
		 * we have to close the server socket; this way
		 * the accept call will return an error */
		sceNetSocketClose(server_sockfd);

		/* Wait until the server threads ends */
		sceKernelWaitThreadEnd(server_thid, NULL, NULL);

		/* To close the clients we have to do the same:
		 * we have to iterate over all the clients
		 * and shutdown their sockets */
		client_list_thread_end();

		/* Delete the client list mutex */
		sceKernelDeleteMutex(client_list_mtx);

		client_list = NULL;
		number_clients = 0;

		if (netctl_init == 0)
			sceNetCtlTerm();
		if (net_init == 0)
			sceNetTerm();
		if (net_memory)
			sce_paf_free(net_memory);

		netctl_init = -1;
		net_init = -1;
		net_memory = NULL;
		ftp_initialized = 0;
	}
}

int ftpvita_is_initialized()
{
	return ftp_initialized;
}

int ftpvita_add_device(const char *devname)
{
	int i;
	for (i = 0; i < MAX_DEVICES; i++) {
		if (!device_list[i].valid) {
			sce_paf_strcpy(device_list[i].name, devname);
			device_list[i].valid = 1;
			return 1;
		}
	}
	return 0;
}

int ftpvita_del_device(const char *devname)
{
	int i;
	for (i = 0; i < MAX_DEVICES; i++) {
		if (sce_paf_strcmp(devname, device_list[i].name) == 0) {
			device_list[i].valid = 0;
			return 1;
		}
	}
	return 0;
}

void ftpvita_set_info_log_cb(ftpvita_log_cb_t cb)
{
	info_log_cb = cb;
}

void ftpvita_set_debug_log_cb(ftpvita_log_cb_t cb)
{
	debug_log_cb = cb;
}

void ftpvita_set_file_buf_size(unsigned int size)
{
	file_buf_size = size;
}

int ftpvita_ext_add_custom_command(const char *cmd, cmd_dispatch_func func)
{
	int i;
	for (i = 0; i < MAX_CUSTOM_COMMANDS; i++) {
		if (!custom_command_dispatchers[i].valid) {
			custom_command_dispatchers[i].cmd = cmd;
			custom_command_dispatchers[i].func = func;
			custom_command_dispatchers[i].valid = 1;
			return 1;
		}
	}
	return 0;
}

int ftpvita_ext_del_custom_command(const char *cmd)
{
	int i;
	for (i = 0; i < MAX_CUSTOM_COMMANDS; i++) {
		if (sce_paf_strcmp(cmd, custom_command_dispatchers[i].cmd) == 0) {
			custom_command_dispatchers[i].valid = 0;
			return 1;
		}
	}
	return 0;
}

void ftpvita_ext_client_send_ctrl_msg(ftpvita_client_info_t *client, const char *msg)
{
	client_send_ctrl_msg(client, msg);
}

void ftpvita_ext_client_send_data_msg(ftpvita_client_info_t *client, const char *str)
{
	client_send_data_msg(client, str);
}
