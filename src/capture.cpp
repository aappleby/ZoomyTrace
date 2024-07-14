#include "capture.hpp"

#include <sys/epoll.h>

#define USB_CONFIGURATION	1
#define SALEAE_VID 0x0925
#define SALEAE_PID 0x3881
#define FW_CHUNKSIZE (4 * 1024)

#define CHECK(func) { auto ret = (func); if (ret < 0) { printf("libusberror %s@%d = %s\n", __FILE__, __LINE__, libusb_error_name(ret)); return ret; } }

extern unsigned char fx2lafw_saleae_logic_fw[];
unsigned int fx2lafw_saleae_logic_fw_len = 8120;

//#define LIBUSB_DEBUG

//cmd_start_acquisition cmd = {CMD_START_FLAGS_SAMPLE_8BIT | CMD_START_FLAGS_CLK_48MHZ, 0, 1};

/*
		if (libusb_has_capability(LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER)) {
			if (libusb_kernel_driver_active(usb->devhdl, USB_INTERFACE) == 1) {
				if ((ret = libusb_detach_kernel_driver(usb->devhdl, USB_INTERFACE)) < 0) {
					sr_err("Failed to detach kernel driver: %s.",
						libusb_error_name(ret));
					ret = SR_ERR;
					break;
				}
			}
		}
*/

const char* capcmd_to_cstr(CapCommand cmd) {
  switch(cmd) {
  case XCMD_CONNECT:   return "XCMD_CONNECT";
  case XCMD_START_CAP: return "XCMD_START_CAP";
  case XCMD_BLOCK:     return "XCMD_BLOCK";
  case XCMD_STOP_CAP:  return "XCMD_STOP_CAP";
  case XCMD_GET_FWID:  return "XCMD_GET_FWID";
  case XCMD_GET_REVID: return "XCMD_GET_REVID";
  case XCMD_DISCONNECT:return "XCMD_DISCONNECT";
  case XCMD_TERMINATE: return "XCMD_TERMINATE";
  default: return "<error>";
  }
}

//------------------------------------------------------------------------------

Capture::Capture() {
  log("Capture()");

  device_present = false;
  capture_thread = nullptr;
  ctx = nullptr;
  hdev = nullptr;

  // Allocate buffers & packets
  //ring_buffer = new uint8_t[ring_size];
  ring_buffer = (uint8_t*)aligned_alloc(chunk_size, ring_size);
  ring_buffer_len = ring_size;

  for (int i = 0; i < 16; i++) {
    auto t = libusb_alloc_transfer(0);
    t->buffer = new uint8_t[8 + 4096];
    control_pool.push(t);
  }

  for (int i = 0; i < 16; i++) {
    auto t = libusb_alloc_transfer(0);
    bulk_pool.push(t);
  }

  int ret = 0;

  // Init libusb
  log("libusb_init()");
  ret = libusb_init(&ctx);
  assert(ret == 0);


#ifdef LIBUSB_DEBUG
  log("libusb_set_option(DEBUG)");
  libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
  //libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);
#endif

  // Init hotplug callback
  ret = libusb_hotplug_register_callback(
    ctx,
    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
    LIBUSB_HOTPLUG_ENUMERATE,
    LIBUSB_HOTPLUG_MATCH_ANY,
    LIBUSB_HOTPLUG_MATCH_ANY,
    LIBUSB_HOTPLUG_MATCH_ANY,
    [](libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void* user_data) -> int {
      Capture* cap = (Capture*)user_data;
      //return ((Capture*)user_data)->on_hotplug(device, event);

      struct libusb_device_descriptor desc;
      libusb_get_device_descriptor(device, &desc);

      if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        if (desc.idVendor == SALEAE_VID && desc.idProduct == SALEAE_PID) {
          log("Device 0x%04x:0x%04x arrived", desc.idVendor, desc.idProduct);
          cap->device_present = true;
        }
      }

      if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        if (desc.idVendor == SALEAE_VID && desc.idProduct == SALEAE_PID) {
          log("Device 0x%04x:0x%04x left", desc.idVendor, desc.idProduct);
          cap->device_present = false;
        }
      }

      return 0;
    },
    this,
    &hotplug_handle
  );
  assert(ret == 0);
}

//------------------------------------------------------------------------------

Capture::~Capture() {
  log("~Capture()");
  assert(hdev == nullptr);

  // Deinit hotplug callback
  libusb_hotplug_deregister_callback(ctx, hotplug_handle);

  // Deinit libusb
  libusb_exit(ctx);
  ctx = nullptr;

  // Free buffers & packets
  //delete [] ring_buffer;
  free(ring_buffer);
  ring_buffer = nullptr;

  while (!control_pool.empty()) {
    auto t = control_pool.front();
    control_pool.pop();
    delete [] ((uint8_t*)t->buffer);
    libusb_free_transfer(t);
  }

  while (!bulk_pool.empty()) {
    auto t = bulk_pool.front();
    bulk_pool.pop();
    libusb_free_transfer(t);
  }
}

//------------------------------------------------------------------------------

int Capture::connect() {
  if (hdev) return 0;

  log("Capture::connect()");

  log("libusb_open_device_with_vid_pid()");
  hdev = libusb_open_device_with_vid_pid(ctx, SALEAE_VID, SALEAE_PID);

  if (!hdev) {
    log("Could not open device");
    return -1;
  }

  //----------------------------------------
  // Check device descriptor to see if we need to upload firmware

  log("libusb_get_device()");
  auto dev = libusb_get_device(hdev);
	libusb_device_descriptor des;
  CHECK(libusb_get_device_descriptor(dev, &des));

  bool no_firmware = false;

	unsigned char strdesc[64];
  strdesc[0] = 0;
	if (libusb_get_string_descriptor_ascii(hdev, des.iProduct, strdesc, sizeof(strdesc)) > 0) {
    log("iProduct %s", strdesc);
  }
  else {
    log("Could not get iProduct");
    no_firmware = true;
  }

  strdesc[0] = 0;
	if (libusb_get_string_descriptor_ascii(hdev, des.iManufacturer, strdesc, sizeof(strdesc)) > 0) {
    log("iManufacturer '%s'", strdesc);
  }
  else {
    log("Could not get iManufacturer");
    no_firmware = true;
  }

  //----------------------------------------

  //CHECK(libusb_set_configuration(hdev, USB_CONFIGURATION));
  //CHECK(libusb_claim_interface(hdev, 0));
  libusb_set_configuration(hdev, USB_CONFIGURATION);
  libusb_claim_interface(hdev, 0);

  //----------------------------------------
  // Upload firmware if needed

  //no_firmware = true;

  if (no_firmware) {
    log("Halting CPU");
    int halt = 1;
    CHECK(ezusb_put_sync(EZUSB_HALT_REG_ADDR, &halt, 1));

    log("Uploading firmware");

    int offset = 0x0000;
    unsigned char* firmware = fx2lafw_saleae_logic_fw;
    int length = fx2lafw_saleae_logic_fw_len;

    while (offset < length) {
      int chunksize = std::min(length - offset, FW_CHUNKSIZE);

      int ret = ezusb_put_sync(offset, firmware + offset, chunksize);
      if (ret < 0) {
        err("Unable to send firmware to device: %s.", libusb_error_name(ret));
        break;
      }
      log("Uploaded %u bytes.", chunksize);
      offset += chunksize;
    }

    log("Resuming CPU");
    // Don't check here, this can fail.
    halt = 0;
    ezusb_put_sync(EZUSB_HALT_REG_ADDR, &halt, 1);

    log("Closing device handle");
    disconnect();

    // Device will fall off the bus about 50 milliseconds after this.
    log("Waiting for disconnect");
    while (device_present) {
      timeval tv = { .tv_sec = 1, .tv_usec = 0 };
      int completed = 0;
      libusb_handle_events_timeout_completed(ctx, &tv, &completed);
    }

    // Device will take around 2 seconds to reconnect
    log("Waiting for reconnect");
    while (!device_present) {
      timeval tv = { .tv_sec = 1, .tv_usec = 0 };
      int completed = 0;
      libusb_handle_events_timeout_completed(ctx, &tv, &completed);
    }

    log("Reopening device");
    hdev = libusb_open_device_with_vid_pid(ctx, SALEAE_VID, SALEAE_PID);

    if (!hdev) {
      err("Could not open device");
      return -1;
    }

    CHECK(libusb_claim_interface(hdev, 0));
  }

  log("Capture::connect() done");
  return 0;
}

//------------------------------------------------------------------------------

int Capture::disconnect() {
  if (hdev) {
    log("Capture::disconnect()");
    CHECK(libusb_release_interface(hdev, 0));
    libusb_close(hdev);
    hdev = nullptr;
  }
  return 0;
}

//------------------------------------------------------------------------------

void Capture::start_thread() {
  log("Capture::start_thread()");
  assert(hdev == nullptr);
  assert(capture_thread == nullptr);

  capture_thread = new std::thread([this] () -> void {
    log("Capture::capture_thread_main()");
    thread_init();
    while(thread_running) {
      thread_loop();
    }
    log("Capture::capture_thread_main() exiting");
  });
}

//------------------------------------------------------------------------------

void Capture::stop_thread() {
  log("Capture::stop_thread()");
  assert(capture_thread);

  CapMessage disconnect(XCMD_DISCONNECT, 0,0, 0);
  CapMessage terminate(XCMD_TERMINATE, 0, 0, 0);

  host_to_cap.put(disconnect);
  host_to_cap.put(terminate);
  capture_thread->join();
  assert(hdev == nullptr);
  delete capture_thread;
  capture_thread = nullptr;
}


int Capture::thread_init() {

  thread_running = true;

  //----------

  // And we set up callbacks to maintain the libusb list.
  libusb_set_pollfd_notifiers(
    ctx,

    // FD add callback
    [](int fd, short events, void* user_data) -> void {
      Capture* cap = (Capture*)user_data;
      epoll_event e;
      e.events = events;
      e.data.fd = fd;
      epoll_ctl(cap->epoll_fd, EPOLL_CTL_ADD, fd, &e);
    },

    // FD remove callback
    [](int fd, void* user_data) -> void {
      Capture* cap = (Capture*)user_data;
      epoll_ctl(cap->epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    },

    // user_data
    this
  );

  //----------
  // Create epoll

  {
    epoll_fd = epoll_create1(0);
    assert(epoll_fd);

    epoll_event e;
    e.events = EPOLLIN | EPOLLERR | EPOLLRDHUP;
    e.data.fd = host_to_cap.fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, host_to_cap.fd, &e);

    {
      const libusb_pollfd** pollfds = libusb_get_pollfds(ctx);
      for (auto cursor = pollfds; *cursor; cursor++) {
        epoll_event e;
        //e.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLRDHUP;
        e.events = EPOLLIN | EPOLLERR | EPOLLRDHUP;
        e.data.fd = (*cursor)->fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, (*cursor)->fd, &e);
      }
      libusb_free_pollfds(pollfds);
    }
  }

  return 0;
}

//------------------------------------------------------------------------------

int Capture::thread_loop() {
  //log("thread_loop()");

  // Wait for _either_ a message from the host (fds slot 0) or a USB event
  // (the rest of the slots)

  epoll_event events[16];
  int ret = epoll_wait(epoll_fd, events, 16, -1);

  // Give libusb a chance to handle events first since it is the most time-sensitive bit.
  timeval tv = {0};
  CHECK(libusb_handle_events_timeout_completed(ctx, &tv, nullptr));

  // Libusb's caught up, handle messages from the host.
  while (!host_to_cap.empty()) {
    auto msg = host_to_cap.get();

    //log("-> %-16s 0x%08x 0x%016x %ld", capcmd_to_cstr(msg.command), msg.result, msg.block, msg.length);


    switch(msg.command) {

      case XCMD_CONNECT: {
        msg.result = connect();
        cap_to_host.put(msg);
      } break;

      case XCMD_START_CAP: {
        start_cap(msg.result);
        cap_to_host.put(msg);
      } break;

      case XCMD_BLOCK: {
        // FIXME this message is cap-to-host only
        assert(false);
      } break;

      case XCMD_STOP_CAP: {
        // FIXME does the fx2law firmware have a stop command?
        stop_cap();
      } break;

      case XCMD_GET_FWID: {
        get_fwid();
      } break;

      case XCMD_GET_REVID: {
        get_revid();
      } break;

      case XCMD_DISCONNECT: {
        msg.result = disconnect();
        cap_to_host.put(msg);
      } break;

      case XCMD_TERMINATE: {
        thread_running = false;
        cap_to_host.put(msg);
      } break;
    }
  }

  return 0;
}

//------------------------------------------------------------------------------

int Capture::thread_cleanup() {
  close(epoll_fd);
  epoll_fd = -1;
  return 0;
}

//------------------------------------------------------------------------------

int Capture::ezusb_put_sync(int offset, void* buf, int len) {
	int ret = libusb_control_transfer(
    hdev,
    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
    FW_CMD_EZUSB,
    offset,
    0x0000,
    (unsigned char*)buf,
    len,
    DEFAULT_TIMEOUT
  );
	return ret;
}

//------------------------------------------------------------------------------

int Capture::get_fwid() {
  int fwid = 0;
  int ret = libusb_control_transfer(
    hdev,
    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
    FW_CMD_GET_FWID,
    0x0000,
    0x0000,
    (unsigned char*)&fwid,
    2,
    100);
  cap_to_host.put({XCMD_GET_FWID, fwid, 0, 0});
  return ret;
}

int Capture::get_revid() {
  int revid = 0;
  int ret = libusb_control_transfer(
    hdev,
    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
    FW_CMD_GET_REVID,
    0x0000,
    0x0000,
    (unsigned char*)&revid,
    1,
    100);
  cap_to_host.put({XCMD_GET_REVID, revid, 0, 0});
  return ret;
}

//------------------------------------------------------------------------------

int Capture::start_cap(int block_count) {
  if (block_count == 0) return 0;

  assert(!capture_running);
  bulk_requested = block_count;
  bulk_submitted = 0;
  bulk_pending   = 0;
  bulk_done      = 0;

  auto transfer = control_pool.front();
  control_pool.pop();

  libusb_fill_control_setup(
    transfer->buffer,
    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
    FW_CMD_START,
    0x0000,
    0x0000,
    sizeof(cmd_start_acquisition)
  );

  cmd_start_acquisition cmd = {CMD_START_FLAGS_SAMPLE_8BIT | CMD_START_FLAGS_CLK_48MHZ, 0, 1};
  memcpy(transfer->buffer + 8, &cmd, sizeof(cmd));

  libusb_fill_control_transfer(
    transfer,
    hdev,
    transfer->buffer,
    [](libusb_transfer* transfer) -> void {
      Capture* cap = (Capture*)transfer->user_data;
      cap->control_pool.push(transfer);
    },
    this,
    100);

  CHECK(libusb_submit_transfer(transfer));

  // We _must_ immediately enqueue _two_ bulk transfers after the cap starts or
  // the FX2's internal buffer will overflow.

  if (block_count > 0) CHECK(queue_chunk());
  if (block_count > 1) CHECK(queue_chunk());

  capture_running = true;

  return 0;
}

//------------------------------------------------------------------------------

int Capture::stop_cap() {
  bulk_requested = 0;
  capture_running = false;
  return 0;
}

//------------------------------------------------------------------------------

int Capture::queue_chunk() {
  auto transfer = bulk_pool.front();
  bulk_pool.pop();

  auto ring = ring_buffer + ring_write;

  libusb_fill_bulk_transfer(
    transfer,
    hdev,
    BULK_ENDPOINT | LIBUSB_ENDPOINT_IN,
    ring,
    chunk_size,
    [](libusb_transfer* transfer) -> void {
      //log("chunk received");
      Capture* cap = (Capture*)transfer->user_data;
      cap->ring_ready = (cap->ring_ready + cap->chunk_size) & cap->ring_mask;
      cap->bulk_pending--;
      cap->bulk_done++;
      cap->bulk_pool.push(transfer);

      cap->cap_to_host.put({XCMD_BLOCK, 0, transfer->buffer, cap->chunk_size});

      if (cap->bulk_submitted < cap->bulk_requested) {
        //log("queueing next chunk");
        cap->queue_chunk();
      }

      if (cap->bulk_done >= cap->bulk_requested) {
        //log("capture done");
        cap->capture_running = false;
        cap->cap_to_host.put({XCMD_STOP_CAP, cap->bulk_done, 0, 0});

      }

    },
    this,
    1000);

  CHECK(libusb_submit_transfer(transfer));
  ring_write = (ring_write + chunk_size) & ring_mask;
  bulk_submitted++;
  bulk_pending++;

  return 0;
}

//------------------------------------------------------------------------------
