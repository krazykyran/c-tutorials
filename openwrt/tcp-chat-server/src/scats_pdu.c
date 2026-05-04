#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libubox/uloop.h>

#define SCATS_FLAG_CODE 0x7e
#define SCATS_ESC_CODE 0x7d
#define SCATS_ESC_XOR 0x20

static void hexdump(const uint8_t *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		printf("%02x", buf[i]);
		if ((i + 1) % 16 == 0 || i + 1 == len)
			printf("\n");
		else
			printf(" ");
	}
}

struct byte_buffer {
	uint8_t *data;
	size_t len;
	size_t cap;
};

static int bb_append(struct byte_buffer *buf, uint8_t value)
{
	if (buf->len == buf->cap) {
		size_t new_cap = buf->cap ? buf->cap * 2 : 32;
		uint8_t *new_data = realloc(buf->data, new_cap);

		if (!new_data)
			return -1;
		buf->data = new_data;
		buf->cap = new_cap;
	}

	buf->data[buf->len++] = value;
	return 0;
}

static int bb_append_slice(struct byte_buffer *buf, const uint8_t *data, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (bb_append(buf, data[i]) < 0)
			return -1;
	}
	return 0;
}

static void bb_free(struct byte_buffer *buf)
{
	free(buf->data);
	buf->data = NULL;
	buf->len = 0;
	buf->cap = 0;
}

static uint16_t scats_crc_ccitt(const uint8_t *data, size_t len, uint16_t init_value,
				uint16_t xor_value)
{
	static const uint16_t table[256] = {
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108,
		0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
		0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b,
		0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
		0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee,
		0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
		0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d,
		0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 0x5af5,
		0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
		0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87, 0x4ce4,
		0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
		0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13,
		0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
		0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e,
		0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1,
		0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
		0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3, 0x14a0,
		0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
		0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657,
		0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
		0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882,
		0x28a3, 0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 0xfd2e,
		0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
		0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1, 0xef1f, 0xff3e, 0xcf5d,
		0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
		0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
	};
	size_t i;
	uint16_t crc = init_value;

	for (i = 0; i < len; i++)
		crc = (uint16_t)((crc << 8) ^ table[((crc >> 8) ^ data[i]) & 0xff]);

	return (uint16_t)(crc ^ xor_value);
}

static int scats_append_escaped_byte(struct byte_buffer *encoded, uint8_t b)
{
	if (b == SCATS_FLAG_CODE || b == SCATS_ESC_CODE) {
		if (bb_append(encoded, SCATS_ESC_CODE) < 0 ||
		    bb_append(encoded, b ^ SCATS_ESC_XOR) < 0)
			return -1;
		return 0;
	}

	return bb_append(encoded, b);
}

static int scats_encode_hdlc(const uint8_t *payload, size_t payload_len, uint8_t type,
			     struct byte_buffer *encoded)
{
	struct byte_buffer pdu = { 0 };
	uint16_t crc;
	size_t i;

	/* SCATS framing: [type][message][crc_hi][crc_lo], then escape transform. */
	if (bb_append(&pdu, type) < 0 || bb_append_slice(&pdu, payload, payload_len) < 0) {
		bb_free(&pdu);
		return -1;
	}

	crc = scats_crc_ccitt(pdu.data, pdu.len, 0x0000, 0x0000);
	if (bb_append(&pdu, (uint8_t)((crc >> 8) & 0xff)) < 0 ||
	    bb_append(&pdu, (uint8_t)(crc & 0xff)) < 0) {
		bb_free(&pdu);
		return -1;
	}

	for (i = 0; i < pdu.len; i++) {
		if (scats_append_escaped_byte(encoded, pdu.data[i]) < 0) {
			bb_free(&pdu);
			return -1;
		}
	}

	bb_free(&pdu);
	return 0;
}

static int scats_trans_decode(const uint8_t *data, size_t data_len, struct byte_buffer *decoded)
{
	size_t i;

	for (i = 0; i < data_len; i++) {
		uint8_t item = data[i];

		if (item != SCATS_ESC_CODE) {
			if (bb_append(decoded, item) < 0)
				return -1;
			continue;
		}

		if (i + 1 >= data_len)
			return -1;

		i++;
		switch (data[i]) {
		case (SCATS_FLAG_CODE ^ SCATS_ESC_XOR):
			if (bb_append(decoded, SCATS_FLAG_CODE) < 0)
				return -1;
			break;
		case (SCATS_ESC_CODE ^ SCATS_ESC_XOR):
			if (bb_append(decoded, SCATS_ESC_CODE) < 0)
				return -1;
			break;
		default:
			return -1;
		}
	}

	return 0;
}

static int scats_decode_hdlc(const uint8_t *frame, size_t frame_len, uint8_t *type_out,
			     struct byte_buffer *message_out)
{
	struct byte_buffer pdu = { 0 };
	uint16_t crc_frame;
	uint16_t crc_calc;
	size_t pdu_len;

	if (!frame || frame_len < 2 || frame[0] != SCATS_FLAG_CODE ||
	    frame[frame_len - 1] != SCATS_FLAG_CODE)
		return -1;

	if (scats_trans_decode(frame + 1, frame_len - 2, &pdu) < 0) {
		bb_free(&pdu);
		return -1;
	}

	pdu_len = pdu.len;
	if (pdu_len < 4) {
		bb_free(&pdu);
		return -1;
	}

	*type_out = pdu.data[0];
	if (bb_append_slice(message_out, pdu.data + 1, pdu_len - 3) < 0) {
		bb_free(&pdu);
		return -1;
	}

	crc_frame = (uint16_t)(((uint16_t)pdu.data[pdu_len - 2] << 8) | pdu.data[pdu_len - 1]);
	crc_calc = scats_crc_ccitt(pdu.data, pdu_len - 2, 0x0000, 0x0000);
	bb_free(&pdu);

	if (crc_frame != crc_calc)
		return -1;

	return 0;
}

static int scats_encode_tsc_sync_message(bool hdlc, uint8_t type, const uint8_t *data_bytes,
					 size_t data_len, struct byte_buffer *out_packet)
{
	struct byte_buffer message = { 0 };
	struct byte_buffer hdlc_body = { 0 };
	uint8_t header[] = { 0x01, 0x01, 0x01, 0x01 };
	uint8_t mode = hdlc ? 0x00 : 0x80;
	uint8_t trailer = 0x00;

	if (bb_append_slice(&message, header, sizeof(header)) < 0 ||
	    bb_append(&message, mode) < 0 || bb_append(&message, trailer) < 0 ||
	    bb_append_slice(&message, data_bytes, data_len) < 0) {
		bb_free(&message);
		return -1;
	}

	if (!hdlc) {
		if (bb_append_slice(out_packet, message.data, message.len) < 0) {
			bb_free(&message);
			return -1;
		}
		bb_free(&message);
		return 0;
	}

	if (scats_encode_hdlc(message.data, message.len, type, &hdlc_body) < 0) {
		bb_free(&message);
		return -1;
	}

	if (bb_append(out_packet, SCATS_FLAG_CODE) < 0 ||
	    bb_append_slice(out_packet, hdlc_body.data, hdlc_body.len) < 0 ||
	    bb_append(out_packet, SCATS_FLAG_CODE) < 0) {
		bb_free(&hdlc_body);
		bb_free(&message);
		return -1;
	}

	bb_free(&hdlc_body);
	bb_free(&message);
	return 0;
}

static int scats_encode_tsc_id_request(uint8_t device_type, bool hdlc, struct byte_buffer *out_packet)
{
	uint8_t message[] = { 0x01, 0x01, 0x01, 0x01, 0x32, device_type };
	struct byte_buffer hdlc_body = { 0 };
	size_t i;
	int sum = 0;
	uint8_t hdlc_type = 0x00;

	if (hdlc) {
		if (scats_encode_hdlc(message, sizeof(message), hdlc_type, &hdlc_body) < 0)
			return -1;

		if (bb_append(out_packet, SCATS_FLAG_CODE) < 0 ||
		    bb_append_slice(out_packet, hdlc_body.data, hdlc_body.len) < 0 ||
		    bb_append(out_packet, SCATS_FLAG_CODE) < 0) {
			bb_free(&hdlc_body);
			return -1;
		}
		bb_free(&hdlc_body);
		return 0;
	}

	if (bb_append_slice(out_packet, message, sizeof(message)) < 0)
		return -1;

	/* Match Qt logic exactly for non-HDLC parity handling. */
	for (i = 0; i < out_packet->len; i++) {
		int y;

		for (y = 0; y < 8; y++) {
			if (out_packet->data[i] & (1 << y))
				sum++;
		}
	}
	if ((sum & 0x02) == 0)
		out_packet->data[4] |= 0x80;

	return 0;
}

static int scats_decode_tsc_id_response(const uint8_t *packet, size_t packet_len,
					uint8_t *device_type, uint16_t *site_id, bool hdlc)
{
	struct byte_buffer message = { 0 };
	uint8_t type = 0;
	size_t expected_length = hdlc ? 10 : 5;
	size_t i;

	if (!packet || !device_type || !site_id || packet_len < expected_length)
		return -1;

	if (hdlc) {
		size_t flag_start = packet_len;
		size_t flag_end = packet_len;

		for (i = 0; i < packet_len; i++) {
			if (packet[i] == SCATS_FLAG_CODE) {
				flag_start = i;
				break;
			}
		}
		if (flag_start == packet_len)
			return -1;

		for (i = flag_start + 1; i < packet_len; i++) {
			if (packet[i] == SCATS_FLAG_CODE) {
				flag_end = i;
				break;
			}
		}
		if (flag_end == packet_len || (flag_end - flag_start) < 2)
			return -1;

		if (scats_decode_hdlc(packet + flag_start, flag_end - flag_start + 1, &type, &message) < 0) {
			bb_free(&message);
			return -1;
		}
		if (type != 0x00) {
			bb_free(&message);
			return -1;
		}
	} else {
		if (bb_append_slice(&message, packet, packet_len) < 0) {
			bb_free(&message);
			return -1;
		}
	}

	if (message.len < 5) {
		bb_free(&message);
		return -1;
	}

	switch (message.data[0] & 0x7f) {
	case SCATS_FLAG_CODE:
	case 0x10:
	case 0x90:
		bb_free(&message);
		return -1;
	case 0x32:
		if (message.len != 5) {
			bb_free(&message);
			return -1;
		}
		break;
	default:
		bb_free(&message);
		return -1;
	}

	if ((message.data[1] & 0x01) == 0x00) {
		if (*device_type == 0x06) {
			*device_type = 0x07;
			bb_free(&message);
			return -1;
		}
		if (*device_type == 0x07) {
			*device_type = 0x06;
			bb_free(&message);
			return -1;
		}
		bb_free(&message);
		return -1;
	}

	if (message.data[4] == 0x00) {
		*site_id = (uint16_t)(((uint16_t)message.data[2] << 8) | message.data[3]);
		bb_free(&message);
		return 0;
	}

	*device_type = 0x06;
	bb_free(&message);
	return -1;
}

int main(void)
{
	struct byte_buffer packet = { 0 };
	struct byte_buffer decoded_message = { 0 };
	struct byte_buffer tampered_frame = { 0 };
	struct byte_buffer id_request_plain = { 0 };
	struct byte_buffer id_request_hdlc = { 0 };
	struct byte_buffer id_response_hdlc_body = { 0 };
	struct byte_buffer id_response_hdlc_packet = { 0 };
	uint8_t dummy_data_bytes[] = { 0xde, 0xad, 0xbe, 0xef, 0x7e, 0x7d, 0x42 };
	uint8_t id_response_message[] = { 0x32, 0x01, 0x02, 0x09, 0x00 };
	uint8_t type = 0x11;
	uint8_t decoded_type = 0;
	uint8_t id_device_type = 0x06;
	uint16_t id_site = 0;

	if (uloop_init() < 0) {
		fprintf(stderr, "uloop_init failed\n");
		return EXIT_FAILURE;
	}

	printf("Dummy data_bytes (%zu bytes):\n", sizeof(dummy_data_bytes));
	hexdump(dummy_data_bytes, sizeof(dummy_data_bytes));

	if (scats_encode_tsc_sync_message(false, type, dummy_data_bytes, sizeof(dummy_data_bytes),
					  &packet) <
	    0) {
		fprintf(stderr, "Failed to encode non-HDLC packet\n");
		return EXIT_FAILURE;
	}

	printf("\nEncoded packet (hdlc=false):\n");
	hexdump(packet.data, packet.len);
	bb_free(&packet);

	if (scats_encode_tsc_sync_message(true, type, dummy_data_bytes, sizeof(dummy_data_bytes),
					  &packet) <
	    0) {
		fprintf(stderr, "Failed to encode HDLC packet\n");
		return EXIT_FAILURE;
	}

	printf("\nEncoded packet (hdlc=true):\n");
	hexdump(packet.data, packet.len);

	if (scats_decode_hdlc(packet.data, packet.len, &decoded_type, &decoded_message) < 0) {
		fprintf(stderr, "Failed to decode HDLC packet\n");
		bb_free(&packet);
		return EXIT_FAILURE;
	}

	printf("\nDecoded type:\n%02x\n", decoded_type);
	printf("Decoded message (%zu bytes):\n", decoded_message.len);
	hexdump(decoded_message.data, decoded_message.len);
	if (decoded_type != type || decoded_message.len != 6 + sizeof(dummy_data_bytes) ||
	    memcmp(decoded_message.data,
		   (uint8_t[]){ 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,
				0xde, 0xad, 0xbe, 0xef, 0x7e, 0x7d, 0x42 },
		   decoded_message.len) != 0) {
		fprintf(stderr, "Round-trip mismatch\n");
		bb_free(&decoded_message);
		bb_free(&packet);
		return EXIT_FAILURE;
	}
	printf("\nRound-trip check: PASS\n");

	if (bb_append_slice(&tampered_frame, packet.data, packet.len) < 0) {
		fprintf(stderr, "Failed to prepare tampered frame\n");
		bb_free(&decoded_message);
		bb_free(&packet);
		return EXIT_FAILURE;
	}

	/*
	 * Negative test: flip one non-flag byte in-frame and expect CRC failure.
	 * Index 2 is guaranteed non-flag for this test packet.
	 */
	tampered_frame.data[2] ^= 0x01;
	printf("\nTampered packet (one byte flipped):\n");
	hexdump(tampered_frame.data, tampered_frame.len);
	bb_free(&decoded_message);
	decoded_type = 0;
	if (scats_decode_hdlc(tampered_frame.data, tampered_frame.len, &decoded_type,
			      &decoded_message) == 0) {
		fprintf(stderr, "Negative CRC test failed: decode unexpectedly succeeded\n");
		bb_free(&decoded_message);
		bb_free(&tampered_frame);
		bb_free(&packet);
		return EXIT_FAILURE;
	}
	printf("\nNegative CRC test: PASS (decode rejected tampered frame)\n");

	if (scats_encode_tsc_id_request(id_device_type, false, &id_request_plain) < 0) {
		fprintf(stderr, "Failed to encode TSC ID request (non-HDLC)\n");
		bb_free(&decoded_message);
		bb_free(&tampered_frame);
		bb_free(&packet);
		return EXIT_FAILURE;
	}
	printf("\nTSC ID request (hdlc=false):\n");
	hexdump(id_request_plain.data, id_request_plain.len);

	if (scats_encode_tsc_id_request(id_device_type, true, &id_request_hdlc) < 0) {
		fprintf(stderr, "Failed to encode TSC ID request (HDLC)\n");
		bb_free(&id_request_plain);
		bb_free(&decoded_message);
		bb_free(&tampered_frame);
		bb_free(&packet);
		return EXIT_FAILURE;
	}
	printf("\nTSC ID request (hdlc=true):\n");
	hexdump(id_request_hdlc.data, id_request_hdlc.len);

	/* Build an example ID response for site ID 521 (0x0209). */
	if (scats_encode_hdlc(id_response_message, sizeof(id_response_message), 0x00,
			      &id_response_hdlc_body) < 0 ||
	    bb_append(&id_response_hdlc_packet, SCATS_FLAG_CODE) < 0 ||
	    bb_append_slice(&id_response_hdlc_packet, id_response_hdlc_body.data,
			    id_response_hdlc_body.len) < 0 ||
	    bb_append(&id_response_hdlc_packet, SCATS_FLAG_CODE) < 0) {
		fprintf(stderr, "Failed to build HDLC ID response packet\n");
		bb_free(&id_response_hdlc_body);
		bb_free(&id_request_hdlc);
		bb_free(&id_request_plain);
		bb_free(&decoded_message);
		bb_free(&tampered_frame);
		bb_free(&packet);
		return EXIT_FAILURE;
	}
	printf("\nExample TSC ID response frame for site 521:\n");
	hexdump(id_response_hdlc_packet.data, id_response_hdlc_packet.len);

	if (scats_decode_tsc_id_response(id_response_message, sizeof(id_response_message), &id_device_type,
					 &id_site, false) < 0 ||
	    id_site != 521) {
		fprintf(stderr, "Failed to decode TSC ID response (non-HDLC)\n");
		bb_free(&id_response_hdlc_packet);
		bb_free(&id_response_hdlc_body);
		bb_free(&id_request_hdlc);
		bb_free(&id_request_plain);
		bb_free(&decoded_message);
		bb_free(&tampered_frame);
		bb_free(&packet);
		return EXIT_FAILURE;
	}

	id_device_type = 0x06;
	id_site = 0;
	if (scats_decode_tsc_id_response(id_response_hdlc_packet.data, id_response_hdlc_packet.len,
					 &id_device_type, &id_site, true) < 0 ||
	    id_site != 521) {
		fprintf(stderr, "Failed to decode TSC ID response (HDLC)\n");
		bb_free(&id_response_hdlc_packet);
		bb_free(&id_response_hdlc_body);
		bb_free(&id_request_hdlc);
		bb_free(&id_request_plain);
		bb_free(&decoded_message);
		bb_free(&tampered_frame);
		bb_free(&packet);
		return EXIT_FAILURE;
	}
	printf("\nTSC ID decode test: PASS (siteId=%u)\n", id_site);

	bb_free(&decoded_message);
	bb_free(&tampered_frame);
	bb_free(&packet);
	bb_free(&id_response_hdlc_packet);
	bb_free(&id_response_hdlc_body);
	bb_free(&id_request_hdlc);
	bb_free(&id_request_plain);

	uloop_done();

	return EXIT_SUCCESS;
}
