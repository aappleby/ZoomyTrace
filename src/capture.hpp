#pragma once
#include <stdint.h>
#include <atomic>
#include <thread>
#include <libusb-1.0/libusb.h>
#include <assert.h>
#include "log.hpp"
#include "RingBuffer.hpp"
#include "ThreadQueue.hpp"

//------------------------------------------------------------------------------

struct cmd_start_acquisition {
  uint8_t flags;
  uint8_t sample_delay_h;
  uint8_t sample_delay_l;
};

constexpr int CMD_START_FLAGS_CLK_CTL2	    = 0b00010000; // libsigrok enables this if any analog channels are enabled
constexpr int CMD_START_FLAGS_SAMPLE_8BIT	  = 0b00000000;
constexpr int CMD_START_FLAGS_SAMPLE_16BIT	= 0b00100000;
constexpr int CMD_START_FLAGS_CLK_30MHZ	    = 0b00000000;
constexpr int CMD_START_FLAGS_CLK_48MHZ	    = 0b01000000;

constexpr int FW_CMD_EZUSB     = 0xa0;
constexpr int FW_CMD_GET_FWID  = 0xb0;
constexpr int FW_CMD_GET_REVID = 0xb2;
constexpr int FW_CMD_START     = 0xb1;

constexpr int DEFAULT_TIMEOUT = 100;
constexpr int BULK_ENDPOINT = 2;

constexpr uint16_t EZUSB_HALT_REG_ADDR = 0xe600;

//------------------------------------------------------------------------------

enum CapCommand {
  XCMD_CONNECT,     // Connect to the logic analyzer, uploading firmware if needed.
  XCMD_START_CAP,   // Start capturing blocks.
  XCMD_BLOCK,       // Fill a block.
  XCMD_STOP_CAP,    // Stop capturing blocks.
  XCMD_GET_FWID,    //
  XCMD_GET_REVID,   //
  XCMD_DISCONNECT,  // Disconnect from the logic analyzer.
  XCMD_TERMINATE,   // Terminate the capture thread
};

const char* capcmd_to_cstr(CapCommand cmd);

//------------------------------------------------------------------------------

struct CapMessage {

  CapMessage(CapCommand _command, int _result, void* _block, int _length)
  : command(_command), result(_result), block(_block), length(_length)
  {
  }

  CapCommand command;
  int        result;
  void*      block;
  size_t     length;
};

//------------------------------------------------------------------------------

class Capture {
public:

  Capture();
  ~Capture();

  //----------

  void start_thread();
  void stop_thread();

  void post_async(CapMessage req) {
    host_to_cap.put(req);
  }

  CapMessage post_sync(CapMessage req) {
    post_async(req);
    return cap_to_host.get();
  }

  ThreadQueue<CapMessage> host_to_cap;
  ThreadQueue<CapMessage> cap_to_host;

//private:

  int thread_init();
  int thread_loop();
  int thread_cleanup();

  int connect();
  int disconnect();
  int start_cap(int block_count);
  int stop_cap();
  int queue_chunk();
  int get_fwid();
  int get_revid();

  int ezusb_put_sync(int offset, void* buf, int len);

  //----------

  std::atomic_bool thread_running = false;
  std::atomic_bool capture_running = false;

  std::atomic_int  bulk_requested = 0;
  std::atomic_int  bulk_submitted = 0;
  std::atomic_int  bulk_pending = 0;
  std::atomic_int  bulk_done = 0;

  libusb_context* ctx = nullptr;
  libusb_device_handle *hdev = nullptr;
  libusb_hotplug_callback_handle hotplug_handle = -1;

  std::atomic_bool device_present = false;
  std::thread* capture_thread = nullptr;

  // Control transfer packets with a 8b+4k buffer each
  std::queue<libusb_transfer*> control_pool;

  // Bulk transfer packets
  std::queue<libusb_transfer*> bulk_pool;

  int epoll_fd = -1;

  static constexpr int chunk_size = 65536;

  RingBuffer* ring;
};

extern Capture& cap;
