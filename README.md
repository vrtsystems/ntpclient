# NTP Client

This NTP client is similar in nature to `ntpdate` but does not accept any
command line arguments nor does it update the system clock. Note that this NTP
client does not use any NTP libraries but rather works directly at the [NTP
protocol level](http://tools.ietf.org/html/rfc958).

The version here is a port of David Lettier's NTP client to the [OpenThread
6LoWPAN stack](https://openthread.io).

## Usage

The client library can be used in two ways:

### Polling mode

In this mode, we asynchronously submit a NTP request to the desired server and
wait for a response.

```c
/* Forward declaration of handler */
static void ntp_handler(struct ntp_client_t* ntp_client);

int main(void) {
	/* other members go here */
	struct ntp_client_t my_ntp_client;

	/* initialisation happens here */
	memset(&my_ntp_client, 0, sizeof(my_ntp_client));

	while(1) {
		/* main loop code here */

		ntp_client_process(&my_ntp_client);

		if (it_is_time_to_poll && (!my_ntp_client.state)) {
			otIp6Address ntp_address;
			otError err = otIp6AddressFromString(
				"2001:db8:aaaa:bbbb::123", &ntp_address);

			if (err == OT_ERROR_NONE) {
				err = ntp_client_begin(
					/* OpenThread instance */
					sInstance,
					/* NTP client structure */
					&my_ntp_client,
					/* Address and port of server */
					ntp_address, 123,
					/* Time to live */
					255,
					/*
					 * A function to be called with the
					 * response
					 */
					ntp_handler,
					/*
					 * Arbitrary context to be passed
					 * to the handler.  Can be anything
					 * you like, this just sets the
					 * handler_context member.
					 */
					NULL
				);
			}

			if (err != OT_ERROR_NONE) {
				/* Handle the error condition */
			}
		}
	}
	exit 0;
}

static void ntp_handler(struct ntp_client_t* ntp_client) {
	/*
	 * Context is available as ntp_client->handler_context, cast
	 * as you see fit, or ignore it if you wish.
	 */

	/* Check the state */
	switch (ntp_client->state) {
	case NTP_CLIENT_DONE:		/* Request complete */
	case NTP_CLIENT_RECV_BC:	/* Time updated */
		/* Set the system time (for example) */
		settimeofday(&(ntpclient->tv), NULL);
		break;
	case NTP_CLIENT_INT_ERR:	/* We had an internal error */
		/* See ntpclient->error for the reason */
		break;
	case NTP_CLIENT_ERR_TRUNC:
	case NTP_CLIENT_ERR_BC_TRUNC:
	case NTP_CLIENT_COMM_ERR:	/*
					 * There was a problem receiving
					 * the reply (usually unexpected
					 * payload size; too big or small)
					 */
		/* Handle the situation */
		break;
	case NTP_CLIENT_TIMEOUT:	/* No reply received in 30 sec */
		/* Handle the situation */
		break;
	default:
		/* Do nothing */
	}
}

```

### Passive mode

In this mode, you have a NTP server "broadcasting" the time over the mesh.

```c
/* Forward declaration of handler */
static void ntp_handler(struct ntp_client_t* ntp_client);

int main(void) {
	/* other members go here */
	struct ntp_client_t my_ntp_client;

	/* initialisation happens here */
	memset(&my_ntp_client, 0, sizeof(my_ntp_client));

	while(1) {
		/* main loop code here */

		ntp_client_process(&my_ntp_client);

		if (!my_ntp_client.state) {
			otIp6Address ntp_address;
			otError err = otIp6AddressFromString(
				"ff03::101", &ntp_address);

			if (err == OT_ERROR_NONE) {
				err = ntp_client_listen(
					/* OpenThread instance */
					sInstance,
					/* NTP client structure */
					&my_ntp_client,
					/* Address and port of server */
					ntp_address, 123,
					/*
					 * A function to be called with the
					 * response
					 */
					ntp_handler,
					/*
					 * Arbitrary context to be passed
					 * to the handler.  Can be anything
					 * you like, this just sets the
					 * handler_context member.
					 */
					NULL
				);
			} else {
				/* Handle the error condition */
			}
		}
	}
	exit 0;
}

static void ntp_handler(struct ntp_client_t* ntp_client) {
	/*
	 * This is the same as the ntp_handler in the polling case.
	 * You can stop listening by calling:
	 * 	ntp_client_shutdown(ntp_client);
	 */
}

```

## Original Credit

_(C) 2014 David Lettier._
http://www.lettier.com/

## License

See [LICENSE](LICENSE).
