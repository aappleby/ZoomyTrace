#include "capture.hpp"

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#define USB_CONFIGURATION	1

// DMA ring buffer size seems to be limited to 8 megs
#define CHUNK_SIZE  65536
#define CHUNK_COUNT 128
#define RING_SIZE   (CHUNK_SIZE * CHUNK_COUNT)
#define RING_MASK   (RING_SIZE - 1)

uint8_t* ring_buffer = nullptr;
std::atomic_int cursor_write;
std::atomic_int cursor_read;

#define SALEAE_VID 0x0925
#define SALEAE_PID 0x3881

#define CMD_GET_FW_VERSION		        0xb0
#define CMD_GET_REVID_VERSION		      0xb2
#define CMD_START			                0xb1

#define CMD_START_FLAGS_CLK_CTL2	    0b00010000 // libsigrok enables this if any analog channels are enabled
#define CMD_START_FLAGS_SAMPLE_8BIT	  0b00000000
#define CMD_START_FLAGS_SAMPLE_16BIT	0b00100000
#define CMD_START_FLAGS_CLK_30MHZ	    0b00000000
#define CMD_START_FLAGS_CLK_48MHZ	    0b01000000

/*
enum libusb_error {
	LIBUSB_SUCCESS = 0,
	LIBUSB_ERROR_IO = -1,
	LIBUSB_ERROR_INVALID_PARAM = -2,
	LIBUSB_ERROR_ACCESS = -3,
	LIBUSB_ERROR_NO_DEVICE = -4,
	LIBUSB_ERROR_NOT_FOUND = -5,
	LIBUSB_ERROR_BUSY = -6,
	LIBUSB_ERROR_TIMEOUT = -7,
	LIBUSB_ERROR_OVERFLOW = -8,
	LIBUSB_ERROR_PIPE = -9,
	LIBUSB_ERROR_INTERRUPTED = -10,
	LIBUSB_ERROR_NO_MEM = -11,
	LIBUSB_ERROR_NOT_SUPPORTED = -12,
	LIBUSB_ERROR_OTHER = -99
};
*/

//------------------------------------------------------------------------------

static double timestamp() {
  static uint64_t _time_origin = 0;
  timespec ts;
  (void)timespec_get(&ts, TIME_UTC);
  uint64_t now = ts.tv_sec * 1000000000ull + ts.tv_nsec;
  if (!_time_origin) _time_origin = now;
  return double(now - _time_origin) / 1.0e9;
}

//------------------------------------------------------------------------------

uint16_t get_fw_version(libusb_device_handle *hdev) {
  printf("[%10.6f] get_fw_version()\n", timestamp());

  uint16_t version = -1;
	int ret = libusb_control_transfer(
    hdev,
    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
    CMD_GET_FW_VERSION, 0x0000, 0x0000,
    (unsigned char *)&version, sizeof(version),
    100
  );
  assert(ret == sizeof(version));
  printf("[%10.6f] get_fw_version() = 0x%04x\n", timestamp(), version);
	return version;
}

//------------------------------------------------------------------------------

uint16_t get_revid(libusb_device_handle *hdev) {
  printf("[%10.6f] get_revid()\n", timestamp());
  uint8_t revid = -1;
	int ret = libusb_control_transfer(
    hdev,
    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
    CMD_GET_REVID_VERSION, 0x0000, 0x0000,
    (unsigned char *)&revid, sizeof(revid),
    100
  );
  assert(ret == sizeof(revid));
  printf("[%10.6f] get_revid() = 0x%02x\n", timestamp(), revid);
	return revid;
}

//------------------------------------------------------------------------------

void start_acquire(libusb_device_handle* hdev) {
  printf("[%10.6f] start_acquire()\n", timestamp());

  struct cmd_start_acquisition {
    uint8_t flags;
    uint8_t sample_delay_h;
    uint8_t sample_delay_l;
  };

  cmd_start_acquisition cmd = {CMD_START_FLAGS_SAMPLE_8BIT | CMD_START_FLAGS_CLK_48MHZ, 0, 1};

  int ret = libusb_control_transfer(
    hdev,
    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
    CMD_START,
    0x0000,
    0x0000,
    (unsigned char *)&cmd,
    sizeof(cmd),
    100
  );
  assert(ret == sizeof(cmd));

  printf("[%10.6f] start_acquire() = %d\n", timestamp(), ret);
}

//------------------------------------------------------------------------------

void bulk_callback(libusb_transfer *transfer) {
  printf("[%10.6f] bulk_callback()\n", timestamp());

  auto buf = ring_buffer + cursor_read;
  assert(transfer->buffer == buf);

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      printf("%02x ", buf[x + y * 32]);
    }
    printf("\n");
  }

  cursor_read = (cursor_read + CHUNK_SIZE) & RING_MASK;

  libusb_free_transfer(transfer);
}

//------------------------------------------------------------------------------

void fetch_async(libusb_device_handle* hdev) {
  printf("[%10.6f] fetch_async()\n", timestamp());
  int ret = -1;

  libusb_transfer* transfer = libusb_alloc_transfer(0);

  libusb_fill_bulk_transfer(
    transfer,
    hdev,
    2 | LIBUSB_ENDPOINT_IN,
    ring_buffer + cursor_write,
    CHUNK_SIZE,
    bulk_callback,
    nullptr,
    150);

  cursor_write = (cursor_write + CHUNK_SIZE) & RING_MASK;

  ret = libusb_submit_transfer(transfer);
  printf("[%10.6f] fetch_async() = %d\n", timestamp(), ret);

  assert(!ret);
}


//------------------------------------------------------------------------------

bool saw_disconnect = false;
bool saw_reconnect = false;

int my_hotplug_callback(
  libusb_context *ctx,
  libusb_device *device,
  libusb_hotplug_event event,
  void *user_data) {

  struct libusb_device_descriptor desc;
  libusb_get_device_descriptor(device, &desc);

  if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
    if (desc.idVendor == SALEAE_VID && desc.idProduct == SALEAE_PID) {
      printf("Device 0x%04x:0x%04x arrived\n", desc.idVendor, desc.idProduct);
      saw_reconnect = true;
    }
  }

  if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
    if (desc.idVendor == SALEAE_VID && desc.idProduct == SALEAE_PID) {
      printf("Device 0x%04x:0x%04x left\n", desc.idVendor, desc.idProduct);
      saw_disconnect = true;
    }
  }

  return 0;
}


//------------------------------------------------------------------------------

#define CHECK(func) { auto ret = (func); if (ret < 0) { printf("libusberror %s@%d = %s\n", __FILE__, __LINE__, libusb_error_name(ret)); return ret; } }

int ezusb_reset(struct libusb_device_handle *hdl, int set_clear) {
	unsigned char buf[1];
	buf[0] = set_clear ? 1 : 0;
  // Don't check here, this can fail
	int reset_result = libusb_control_transfer(hdl, LIBUSB_REQUEST_TYPE_VENDOR, 0xa0, 0xe600, 0x0000, buf, 1, 100);
  printf("reset_result(%d) = %d\n", set_clear, reset_result);
	return 0;
}

libusb_context* ctx = nullptr;

extern unsigned char fx2lafw_saleae_logic_fw[];
unsigned int fx2lafw_saleae_logic_fw_len = 8120;

#define FW_CHUNKSIZE (4 * 1024)

int capture() {
  int ret = 0;

  printf("libusb_init()\n");
  CHECK(libusb_init(&ctx));
  printf("libusb_set_option()\n");
  //CHECK(libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG));
  //ret = libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);

  CHECK(libusb_hotplug_register_callback(
    ctx,
    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
    LIBUSB_HOTPLUG_ENUMERATE,
    LIBUSB_HOTPLUG_MATCH_ANY,
    LIBUSB_HOTPLUG_MATCH_ANY,
    LIBUSB_HOTPLUG_MATCH_ANY,
    my_hotplug_callback,
    nullptr,
    nullptr
  ));


  printf("libusb_open_device_with_vid_pid()\n");
  libusb_device_handle* hdev = libusb_open_device_with_vid_pid(ctx, SALEAE_VID, SALEAE_PID);

  if (!hdev) {
    printf("Could not open device\n");
    return -1;
  }

  printf("libusb_get_device()\n");
  auto dev = libusb_get_device(hdev);
	libusb_device_descriptor des;
  CHECK(libusb_get_device_descriptor(dev, &des));

  bool no_firmware = false;

	unsigned char strdesc[64];
  strdesc[0] = 0;
	if (libusb_get_string_descriptor_ascii(hdev, des.iProduct, strdesc, sizeof(strdesc)) > 0) {
    printf("iProduct %s\n", strdesc);
  }
  else {
    printf("Could not get iProduct\n");
    no_firmware = true;
  }

  strdesc[0] = 0;
	if (libusb_get_string_descriptor_ascii(hdev, des.iManufacturer, strdesc, sizeof(strdesc)) > 0) {
    printf("iManufacturer '%s'\n", strdesc);
  }
  else {
    printf("Could not get iManufacturer\n");
    no_firmware = true;
  }


	if (libusb_kernel_driver_active(hdev, 0) == 1) {
    printf("Kernel driver active\n");
    //libusb_detach_kernel_driver(usb->devhdl, USB_INTERFACE)
  }
  else {
    printf("Kernel driver not active\n");
  }

  CHECK(libusb_set_configuration(hdev, USB_CONFIGURATION));

  CHECK(libusb_claim_interface(hdev, 0));

  // FIXME
  no_firmware = true;

  if (no_firmware) {

    printf("Halting CPU\n");
    CHECK(ezusb_reset(hdev, 1));

    printf("Uploading firmware\n");

    int offset = 0x0000;
    unsigned char* firmware = fx2lafw_saleae_logic_fw;
    int length = fx2lafw_saleae_logic_fw_len;

    while (offset < length) {
      int chunksize = std::min(length - offset, FW_CHUNKSIZE);

      ret = libusb_control_transfer(hdev, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT, 0xa0, offset, 0x0000, firmware + offset, chunksize, 100);
      if (ret < 0) {
        printf("Unable to send firmware to device: %s.\n", libusb_error_name(ret));
        break;
      }
      printf("Uploaded %u bytes.\n", chunksize);
      offset += chunksize;
    }

    printf("Resuming CPU\n");
    // Don't check here, this can fail
    ezusb_reset(hdev, 0);

    printf("Closing device handle\n");
    CHECK(libusb_release_interface(hdev, 0));
    libusb_close(hdev);
    hdev = nullptr;

    saw_disconnect = false;
    saw_reconnect = false;

    timeval tv = { .tv_sec = 0, .tv_usec = 1000 };
    int completed = 0;

    printf("Waiting for disconnect\n");
    while (!saw_disconnect) {
      libusb_handle_events_timeout_completed(ctx, &tv, &completed);
    }

    printf("Waiting for reconnect\n");
    while (!saw_reconnect) {
      libusb_handle_events_timeout_completed(ctx, &tv, &completed);
    }

    printf("Reopening device\n");
    hdev = libusb_open_device_with_vid_pid(ctx, SALEAE_VID, SALEAE_PID);

    if (!hdev) {
      printf("Could not open device\n");
      return -1;
    }

    CHECK(libusb_claim_interface(hdev, 0));
  }


  get_fw_version(hdev);
  get_revid(hdev);

  /*
  ring_buffer = (uint8_t*)libusb_dev_mem_alloc(hdev, RING_SIZE);
  cursor_read  = 0;
  cursor_write = 0;
  printf("[%10.6f] ring buffer at %p\n", timestamp(), ring_buffer);

  start_acquire(hdev);
  fetch_async(hdev);

  while (cursor_read == 0) {
    timeval tv = { .tv_sec = 0, .tv_usec = 1000 };
    int completed = 0;
    libusb_handle_events_timeout_completed(ctx, &tv, &completed);
  }
  libusb_dev_mem_free(hdev, ring_buffer, RING_SIZE);
  */

  CHECK(libusb_release_interface(hdev, 0));

  printf("Done\n");

  libusb_close(hdev);
  libusb_exit(ctx);
  return 0;
}
