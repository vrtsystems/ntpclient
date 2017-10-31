/* vim: set noet tw=78 si: */
/*!
 * OpenThread NTP Client
 * (C) 2017 VRT Systems <http://www.vrt.com.au>.
 *
 * Based on David Lettier's NTP client
 * (C) 2014 David Lettier <http://www.lettier.com>.
 * Code used with permission.
 */

#include "ntp.h"
#include <string.h>
#include <machine/endian.h>

#define NTP_TIMESTAMP_DELTA	(2208988800ull)

/*!
 * NTP defines the fractional part as 1/2³² seconds or ~233 ps.
 * There are approximately 4295 fractional time units per microsecond.
 */
#define NTP_TS_FRAC_PER_US	(4295)
#define NTP_TIMEOUT		(300)		/*!< Timeout in 0.1 seconds */

/* Forward declaration of reply handler */
static void ntp_client_recv(
		void *context, otMessage *msg,
		const otMessageInfo *msg_info);

/*!
 * Listen for broadcast NTP time updates from an NTP server.
 *
 * @param[inout]	instance	OpenThread instance to use for this
 * 					client's context.
 * @param[inout]	ntp_client	NTP client instance
 * @param[in]		addr		IPv6 address of NTP server
 * @param[in]		port		Port number of NTP server
 * @param[in]		handler		NTP event handler
 * @param[in]		handler_context	NTP event handler context
 */
otError ntp_client_listen(otInstance* instance,
		struct ntp_client_t* const ntp_client,
		const otIp6Address* addr, uint16_t port,
		ntp_client_event_handler_t* handler,
		void* handler_context) {
	/* Validate inputs */
	if (!instance)
		return OT_ERROR_PARSE;
	if (!ntp_client)
		return OT_ERROR_PARSE;
	if (ntp_client->state)
		return OT_ERROR_ALREADY;

	/* Create and zero out the state. */
	memset(ntp_client, 0, sizeof(struct ntp_client_t));

	/* Copy in the instance information */
	ntp_client->instance = instance;

	/*
	 * If multicast, ensure the multicast IP is added.  We don't know if
	 * it's multicast (OpenThread won't tell us), but we can just try it.
	 */
	ntp_client->error = otIp6SubscribeMulticastAddress(instance, addr);
	switch (ntp_client->error) {
	case OT_ERROR_NONE:
		/*
		 * Okay, we're subscribed.
		 * TODO: set flag to unsubscribe later
		 */
		break;
	case OT_ERROR_ALREADY:
		/* Already unsubscribed, don't unsubscribe later. */
		break;
	case OT_ERROR_INVALID_ARGS:
		/* Not a multicast address?  We can ignore this. */
		break;
	default:
		/* We have a problem */
		return ntp_client->error;
	}

	/*
	 * Create a UDP socket, connect to the server, send the packet.
	 */
	ntp_client->error = otUdpOpen(instance, &(ntp_client->socket),
			ntp_client_recv, (void*)ntp_client);
	if (ntp_client->error != OT_ERROR_NONE)
		return ntp_client->error;

	/* Now we're listening */
	ntp_client->state = NTP_CLIENT_LISTEN;
	return ntp_client->error;
}

static otError _ntp_client_shutdown(struct ntp_client_t* const ntp_client) {
	/* Close off the socket, we're done now */
	ntp_client->error = otUdpClose(&(ntp_client->socket));
	if (ntp_client->error != OT_ERROR_NONE) {
		ntp_client->state = NTP_CLIENT_INT_ERR;
		return ntp_client->error;
	}
}

/*!
 * Shutdown a listening client.
 * @param[inout]	ntp_client	NTP client instance
 */
otError ntp_client_shutdown(struct ntp_client_t* const ntp_client) {
	_ntp_client_shutdown(ntp_client);
	if (!ntp_client_is_done(ntp_client)) {
		ntp_client->state = NTP_CLIENT_DONE;
	}
	return ntp_client->error;
}

/*!
 * Initiate a poll of an NTP server.
 *
 * @param[inout]	instance	OpenThread instance to use for this
 * 					client's context.
 * @param[inout]	ntp_client	NTP client instance
 * @param[in]		addr		IPv6 address of NTP server
 * @param[in]		port		Port number of NTP server
 * @param[in]		ttl		Message time-to-live
 */
otError ntp_client_begin(otInstance* instance,
		struct ntp_client_t* const ntp_client,
		const otIp6Address* addr, uint16_t port, uint8_t ttl) {
	/* Validate inputs */
	if (!instance)
		return OT_ERROR_PARSE;
	if (!ntp_client)
		return OT_ERROR_PARSE;
	if (ntp_client->state)
		return OT_ERROR_ALREADY;

	/* Create and zero out the state. */
	memset(ntp_client, 0, sizeof(struct ntp_client_t));

	/* Copy in the instance information */
	ntp_client->instance = instance;

	/*
	 * Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and
	 * mode = 3. The rest will be left set to zero.
	 */
	ntp_client->packet.li = 0;
	ntp_client->packet.vn = 3;
	ntp_client->packet.mode = 3;

	/*
	 * Create a UDP socket, connect to the server, send the packet.
	 */
	ntp_client->error = otUdpOpen(instance, &(ntp_client->socket),
			ntp_client_recv, (void*)ntp_client);
	if (ntp_client->error != OT_ERROR_NONE)
		return ntp_client->error;

	/*
	 * Send it the NTP packet it wants.
	 */

	otMessageInfo msg_info;
	otMessage* msg = otUdpNewMessage(instance, true);
	if (!msg) {
		/* Record new state */
		ntp_client->state = NTP_CLIENT_INT_ERR;
		ntp_client->error = OT_ERROR_NO_BUFS;

		/* Close the socket */
		otUdpClose(&(ntp_client->socket));
		return ntp_client->error;
	}

	memset(&msg_info, 0, sizeof(otMessageInfo));
	memcpy(&(msg_info.mPeerAddr),
			addr, sizeof(otIp6Address));
	msg_info.mPeerPort = port;
	msg_info.mHopLimit = ttl;

	ntp_client->error = otMessageAppend(msg,
			(const uint8_t*)(&(ntp_client->packet)),
			sizeof(struct ntp_packet_t));

	if (ntp_client->error == OT_ERROR_NONE) {
		/* No error, try sending */
		ntp_client->error = otUdpSend(&(ntp_client->socket),
				msg, &msg_info);
	}

	if (ntp_client->error != OT_ERROR_NONE) {
		/* Free message */
		otMessageFree(msg);

		/* Record new state */
		ntp_client->state = NTP_CLIENT_INT_ERR;

		/* Close the socket */
		otUdpClose(&(ntp_client->socket));
		return ntp_client->error;
	}

	/* Wait for the reply to come back */

	ntp_client->timeout = NTP_TIMEOUT;
	ntp_client->state = NTP_CLIENT_SENT;
	return ntp_client->error;
}

/*!
 * Handler of incoming message, just dumps the payload into the local
 * buffer for use later.
 */
static void ntp_client_recv(
		void* context, otMessage* msg,
		const otMessageInfo* msg_info) {
	(void)msg_info;

	struct ntp_client_t* ntp_client = (struct ntp_client_t*)context;

	if ((ntp_client->state != NTP_CLIENT_SENT)
			&& (ntp->client->state != NTP_CLIENT_LISTEN)) {
		/* Invalid state, do nothing */
		return;
	}

	uint16_t recv = otMessageRead(msg, otMessageGetOffset(msg),
			(uint8_t*)(&(ntp_client->packet)),
			sizeof(struct ntp_packet_t));
	if (recv < sizeof(struct ntp_packet_t)) {
		ntp_client->state = (ntp_client->state == NTP_CLIENT_SENT)
				? NTP_CLIENT_ERR_TRUNC
				: NTP_CLIENT_ERR_BC_TRUNC;
	} else {
		ntp_client->state = (ntp_client->state == NTP_CLIENT_SENT)
				? NTP_CLIENT_RECV
				: NTP_CLIENT_RECV_BC;
	}
}

/*!
 * Handling of received data.
 */
static void ntp_client_recv_done(struct ntp_client_t* const ntp_client) {
	if (ntp_client->state == NTP_CLIENT_RECV) {
		/* Close off the socket, we're done now */
		_ntp_client_shutdown(ntp_client);
		if (ntp_client_is_done(ntp_client)) {
			return;
		}
	}

	/*
	 * These two fields contain the time-stamp seconds as the packet left
	 * the NTP server.  The number of seconds correspond to the seconds
	 * passed since 1900.  __ntohl() converts the bit/byte order from the
	 * network's to host's "endianness".
	 */

	ntp_client->packet.txTm_s = __ntohl( ntp_client->packet.txTm_s );
	ntp_client->packet.txTm_f = __ntohl( ntp_client->packet.txTm_f );

	/*
	 * Extract the 32 bits that represent the time-stamp seconds
	 * (since NTP epoch) from when the packet left the server.
	 *
	 * Subtract 70 years worth of seconds from the seconds since 1900.
	 * This leaves the seconds since the UNIX epoch of 1970.
	 */

	ntp_client->tv.tv_sec = (time_t)(
			ntp_client->packet.txTm_s
			- NTP_TIMESTAMP_DELTA);

	/*
	 * Fractional part is in units of 1.0/2³² seconds (~232 ps).  Convert
	 * this to microseconds.
	 */
	ntp_client->tv.tv_usec = ntp_client->packet.txTm_f
		/ NTP_TS_FRAC_PER_US;

	/* If there's a handler, call it now */
	if (ntp_client->handler) {
		(ntp_client->handler)(ntp_client);
	}

	switch (ntp_client->state) {
	case NTP_CLIENT_RECV:
		ntp_client->state = NTP_CLIENT_DONE;
		break;
	case NTP_CLIENT_RECV_BC:
		ntp_client->state = NTP_CLIENT_LISTEN;
		break;
	default:
		break;
	}
}

/*!
 * Handling of timeout
 */
static void ntp_client_recv_timeout(struct ntp_client_t* const ntp_client) {
	/* Close off the socket, we're done now */
	_ntp_client_shutdown(ntp_client);
	if (ntp_client_is_done(ntp_client)) {
		return;
	}

	/* Record the failure */
	ntp_client->state = NTP_CLIENT_TIMEOUT;
}

/*!
 * Process the state of the NTP client.  This should be called in a loop.
 */
void ntp_client_process(struct ntp_client_t* const ntp_client) {
	switch (ntp_client->state) {
	case NTP_CLIENT_SENT:
		if (ntp_client->timeout) {
			ntp_client->timeout--;
		} else {
			/* Timeout reached */
			ntp_client_recv_timeout(ntp_client);
		}
		break;
	case NTP_CLIENT_RECV:
	case NTP_CLIENT_RECV_BC:
		ntp_client_recv_done(ntp_client);
		break;
	case NTP_CLIENT_ERR_TRUNC:
		/*
		 * Received unicast reply was truncated, fail
		 */
		ntp_client->state = NTP_CLIENT_COMM_ERR;
		break;
	case NTP_CLIENT_ERR_BC_TRUNC:
		/*
		 * If there was an error with truncated data, listen for next
		 * message (handler should have seen the error).
		 */
		ntp_client->state = NTP_CLIENT_LISTEN;
		break;
	default:
		/* Do nothing */
		break;
	}
}
