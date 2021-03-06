

/*================================================================
 *
 *
 *   文件名称：tun_socket_notifier.cpp
 *   创 建 者：肖飞
 *   创建日期：2019年11月30日 星期六 22时08分09秒
 *   修改日期：2020年06月14日 星期日 16时05分43秒
 *   描    述：
 *
 *================================================================*/
#include "tun_socket_notifier.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#include "util_log.h"
#include "settings.h"

tun_socket_notifier::tun_socket_notifier(int fd, unsigned int events) : event_notifier(fd, events)
{
	//int flags;
	rx_buffer_received = 0;

	m_init_events = get_events();

	//flags = fcntl(fd, F_GETFL, 0);
	//flags |= O_NONBLOCK;
	//flags = fcntl(fd, F_SETFL, flags);
}

tun_socket_notifier::~tun_socket_notifier()
{
	util_log *l = util_log::get_instance();

	l->printf("%s:%s:%d\n", __FILE__, __PRETTY_FUNCTION__, __LINE__);

	while(!queue_request_data.empty()) {
		request_data_t request_data;
		request_data = queue_request_data.front();
		delete request_data.data;
		queue_request_data.pop();
	}
}

unsigned char tun_socket_notifier::calc_crc8(void *data, size_t size)
{
	unsigned char crc = 0;
	unsigned char *p = (unsigned char *)data;

	while(size > 0) {
		crc += *p;

		p++;
		size--;
	}

	return crc;
}

int tun_socket_notifier::send_request(char *request, int size, struct sockaddr *address, socklen_t address_size)
{
	int ret = -1;
	util_log *l = util_log::get_instance();
	l->printf("invalid %s!\n", __PRETTY_FUNCTION__);
	return ret;
}

int tun_socket_notifier::add_request_data(tun_socket_fn_t fn, void *data, size_t size, struct sockaddr *address, socklen_t address_size)
{
	int ret = 0;
	request_data_t request_data;
	unsigned char *request_data_data;

	request_data_data = new unsigned char[size];

	if(request_data_data == NULL) {
		return ret;
	}

	if(queue_request_data.size() == 0) {
		set_events(m_init_events | POLLOUT);
	}

	memcpy(request_data_data, data, size);

	request_data.fn = fn;
	request_data.data = request_data_data;
	request_data.size = size;
	request_data.address_size = address_size;
	memcpy(&request_data.address, address, address_size);

	queue_request_data.push(request_data);

	ret = size;

	return ret;
}

int tun_socket_notifier::send_request_data()
{
	int ret = 0;
	util_log *l = util_log::get_instance();
	request_data_t request_data;
	unsigned int encrypt_size = SOCKET_TXRX_BUFFER_SIZE;

	request_data = queue_request_data.front();

	//encrypt
	ret = encrypt_request(request_data.data, request_data.size, (unsigned char *)decode_encode_buffer, &encrypt_size);

	if(ret == 0) {
		ret = chunk_sendto(request_data.fn, decode_encode_buffer, encrypt_size, (struct sockaddr *)&request_data.address, request_data.address_size);
	} else {
		l->printf("encrypt request error(%d)!\n", ret);
	}

	delete request_data.data;
	queue_request_data.pop();

	if(queue_request_data.size() == 0) {
		set_events(m_init_events);
	}

	return ret;
}

int tun_socket_notifier::chunk_sendto(tun_socket_fn_t fn, void *data, size_t size, struct sockaddr *address, socklen_t address_size)
{
	int ret = 0;
	request_info_t request_info;

	request_info.fn = (unsigned int)fn;
	request_info.data = (const unsigned char *)data;
	request_info.size = size;
	request_info.consumed = 0;
	request_info.request = (request_t *)tx_buffer;
	request_info.request_size = 0;

	while(request_info.size > request_info.consumed) {
		::request_encode(&request_info);

		if(request_info.request_size != 0) {
			if(send_request(tx_buffer, request_info.request_size, address, address_size) != (int)request_info.request_size) {
				ret = -1;
				break;
			}
		}
	}

	if(ret != -1) {
		ret = size;
	}

	return ret;
}

struct sockaddr *tun_socket_notifier::get_request_address()
{
	util_log *l = util_log::get_instance();
	l->printf("invalid %s!\n", __PRETTY_FUNCTION__);
	return NULL;
}

socklen_t *tun_socket_notifier::get_request_address_size()
{
	util_log *l = util_log::get_instance();
	l->printf("invalid %s!\n", __PRETTY_FUNCTION__);
	return NULL;
}

int tun_socket_notifier::get_domain()
{
	util_log *l = util_log::get_instance();
	l->printf("invalid %s!\n", __PRETTY_FUNCTION__);
	return AF_UNSPEC;
}


void tun_socket_notifier::reply_tun_info()
{
	util_log *l = util_log::get_instance();
	l->printf("invalid %s!\n", __PRETTY_FUNCTION__);
}

void tun_socket_notifier::request_process_hello(unsigned char *data, unsigned int size)
{
	tun_info_t *tun_info = (tun_info_t *)data;
	util_log *l = util_log::get_instance();
	settings *settings = settings::get_instance();
	sockaddr_info_t client_address;
	std::map<sockaddr_info_t, peer_info_t>::iterator it;
	bool log = false;
	peer_info_t peer_info;
	std::string address_string;

	memset(&client_address, 0, sizeof(client_address));

	client_address.domain = get_domain();
	client_address.address_size = *get_request_address_size();
	memcpy(&client_address.address, get_request_address(), client_address.address_size);

	it = settings->map_clients.find(client_address);

	if(it == settings->map_clients.end()) {
		peer_info.tun_info = *tun_info;
		peer_info.fd = this->get_fd();
		peer_info.time = time(NULL);
		settings->map_clients[client_address] = peer_info;
		log = true;
	} else {
		it->second.time = time(NULL);
	}

	reply_tun_info();

	if(log) {
		char address_buffer[128];
		struct sockaddr_in *sin;

		l->printf("update client!\n");
		address_string = settings->get_address_string(client_address.domain, (struct sockaddr *)&client_address.address, &client_address.address_size);
		l->printf("client_address:%s\n", address_string.c_str());

		l->dump((const char *)&tun_info->mac_addr, IFHWADDRLEN);

		memset(address_buffer, 0, sizeof(address_buffer));
		sin = (struct sockaddr_in *)&tun_info->ip;
		inet_ntop(AF_INET, &sin->sin_addr, address_buffer, sizeof(address_buffer));
		l->printf("ip address:%s\n", address_buffer);

		memset(address_buffer, 0, sizeof(address_buffer));
		sin = (struct sockaddr_in *)&tun_info->netmask;
		inet_ntop(AF_INET, &sin->sin_addr, address_buffer, sizeof(address_buffer));
		l->printf("netmask:%s\n", address_buffer);
	}
}


void tun_socket_notifier::request_process_frame(unsigned char *data, unsigned int size)
{
	int ret = -1;
	util_log *l = util_log::get_instance();
	settings *settings = settings::get_instance();
	struct ethhdr *frame_header = (struct ethhdr *)data;
	tun_info_t *tun_info = settings->tun->get_tun_info();
	int unicast_frame = 0;
	int found = 0;
	std::map<sockaddr_info_t, peer_info_t>::iterator it;
	char buffer_mac[32];
	sockaddr_info_t client_address;
	std::string address_string;

	memset(&client_address, 0, sizeof(client_address));

	client_address.domain = get_domain();
	client_address.address_size = *get_request_address_size();
	memcpy(&client_address.address, get_request_address(), client_address.address_size);

	ret = memcmp(frame_header->h_source, tun_info->mac_addr, IFHWADDRLEN);

	if(ret == 0) {
		l->printf("drop frame:source mac address is same as local tun if!\n");
		return;
	}

	ret = memcmp(frame_header->h_dest, tun_info->mac_addr, IFHWADDRLEN);

	if(ret == 0) {
		//l->printf("frame size:%d\n", size);
		ret = write(settings->tun->get_tap_fd(), data, size);

		if(ret < 0) {
			l->printf("write tap device error!(%s)\n", strerror(errno));
		}

		return;
	}

	if((frame_header->h_dest[0] & 0x01) == 0x00) {
		unicast_frame = 1;
	}

	snprintf(buffer_mac, 32, "%02x:%02x:%02x:%02x:%02x:%02x",
	         frame_header->h_dest[0],
	         frame_header->h_dest[1],
	         frame_header->h_dest[2],
	         frame_header->h_dest[3],
	         frame_header->h_dest[4],
	         frame_header->h_dest[5]);

	for(it = settings->map_clients.begin(); it != settings->map_clients.end(); it++) {
		tun_socket_notifier *notifier;
		sockaddr_info_t dest_addr;
		peer_info_t *peer_info;

		dest_addr = it->first;
		peer_info = &it->second;

		notifier = settings->get_notifier(peer_info->fd);

		if(notifier == NULL) {
			continue;
		}

		address_string = settings->get_address_string(dest_addr.domain, (struct sockaddr *)&dest_addr.address, &dest_addr.address_size);

		if(unicast_frame == 0) {
			if(it == settings->map_clients.find(client_address)) {
				l->printf("skip relay frame to source client:%s\n", address_string.c_str());
				continue;
			}

			l->printf("relay frame to %s, frame mac:%s\n", address_string.c_str(), buffer_mac);
			ret = notifier->add_request_data(FN_FRAME, data, size, (struct sockaddr *)&dest_addr.address, dest_addr.address_size);
		} else {
			if(memcmp(frame_header->h_dest, peer_info->tun_info.mac_addr, IFHWADDRLEN) == 0) {
				l->printf("relay frame to %s, frame mac:%s\n", address_string.c_str(), buffer_mac);
				ret = notifier->add_request_data(FN_FRAME, data, size, (struct sockaddr *)&dest_addr.address, dest_addr.address_size);
				found = 1;
				break;
			}
		}
	}

	if(unicast_frame == 0) {
		l->printf("write broadcast/multicast frame, frame mac:%s\n", buffer_mac);
		ret = write(settings->tun->get_tap_fd(), data, size);

		if(ret < 0) {
			l->printf("write tap device error!(%s)\n", strerror(errno));
		}
	} else {
		if(found == 0) {
			l->printf("write unknow frame, frame mac:%s\n", buffer_mac);
			ret = write(settings->tun->get_tap_fd(), data, size);

			if(ret < 0) {
				l->printf("write tap device error!(%s)\n", strerror(errno));
			}
		}
	}
}

void tun_socket_notifier::request_process(request_t *request)
{
	int ret = 0;
	tun_socket_fn_t fn = (tun_socket_fn_t)request->payload.fn;
	unsigned int decrypt_size = SOCKET_TXRX_BUFFER_SIZE;
	util_log *l = util_log::get_instance();

	if(request->header.data_size != request->header.total_size) {
		return;
	}

	//decrypt
	ret = decrypt_request((unsigned char *)(request + 1), request->header.total_size, (unsigned char *)decode_encode_buffer, &decrypt_size);

	if(ret != 0) {
		l->printf("decrypt request error(%d)!\n", ret);
		return;
	}

	switch(fn) {
		case FN_HELLO: {
			request_process_hello((unsigned char *)decode_encode_buffer, decrypt_size);
		}
		break;

		case FN_FRAME: {
			request_process_frame((unsigned char *)decode_encode_buffer, decrypt_size);
		}
		break;

		default: {
		}
		break;
	}
}

void tun_socket_notifier::process_message()
{
	char *request = NULL;
	char *buffer = rx_buffer;
	//util_log *l = util_log::get_instance();
	int request_size = 0;
	int left = rx_buffer_received;

	//l->printf("left %d bytes\n", left);
	//l->dump((const char *)rx_buffer, left);

	while(left >= (int)sizeof(request_t)) {
		::request_decode(buffer, left, &request, &request_size);
		//l->printf("got request_size %d\n", request_size);

		if(request != NULL) {//可能有效包
			if(request_size != 0) {//有效包
				request_process((request_t *)request);
				buffer += request_size;
				left -= request_size;
			} else {//还要收,退出包处理
				break;
			}
		} else {//没有有效包
			buffer += 1;
			left -= 1;
		}

		//l->printf("left %d bytes\n", left);
	}

	if(left > 0) {
		if(rx_buffer != buffer) {
			memmove(rx_buffer, buffer, left);
		}
	}

	rx_buffer_received = left;
}

int tun_socket_notifier::encrypt_request(unsigned char *in_data, unsigned int in_size, unsigned char *out_data, unsigned int *out_size)
{
	int ret = 0;
	unsigned int i;
	unsigned char mask = 0xff;
	//long unsigned int size;

	//size = *out_size;
	//*out_size = 0;
	//ret = compress(out_data, &size, in_data, in_size);

	//if (ret == Z_OK) {
	//	*out_size = size;
	//}

	//for(i = 0; i < size; i++) {
	//	out_data[i] = out_data[i] ^ mask;
	//}

	*out_size = in_size;

	for(i = 0; i < in_size; i++) {
		out_data[i] = in_data[i] ^ mask;
	}

	return ret;
}

int tun_socket_notifier::decrypt_request(unsigned char *in_data, unsigned int in_size, unsigned char *out_data, unsigned int *out_size)
{
	int ret = 0;
	unsigned int i;
	unsigned char mask = 0xff;
	//long unsigned int size;

	//size = *out_size;
	//*out_size = 0;

	//for(i = 0; i < in_size; i++) {
	//	in_data[i] = in_data[i] ^ mask;
	//}

	//ret = uncompress(out_data, &size, in_data, in_size);

	//if (ret == Z_OK) {
	//	*out_size = size;
	//}

	*out_size = in_size;

	for(i = 0; i < in_size; i++) {
		out_data[i] = in_data[i] ^ mask;
	}

	return ret;
}

void tun_socket_notifier::check_client()
{
	settings *settings = settings::get_instance();
	util_log *l = util_log::get_instance();
	std::map<sockaddr_info_t, peer_info_t>::iterator it;
	std::vector<sockaddr_info_t> invalid_address;
	std::vector<sockaddr_info_t>::iterator it_address;
	peer_info_t peer_info;
	sockaddr_info_t address;
	time_t current_time = time(NULL);
	std::string address_string;

	for(it = settings->map_clients.begin(); it != settings->map_clients.end(); it++) {
		address = it->first;
		peer_info = it->second;

		if(current_time - peer_info.time >= CLIENT_VALIDE_TIMEOUT) {
			invalid_address.push_back(address);
		}
	}

	for(it_address = invalid_address.begin(); it_address != invalid_address.end(); it_address++) {
		address = *it_address;

		settings->map_clients.erase(address);

		address_string = settings->get_address_string(address.domain, (struct sockaddr *)&address.address, &address.address_size);

		l->printf("remove inactive client:%s!\n", address_string.c_str());
	}
}
