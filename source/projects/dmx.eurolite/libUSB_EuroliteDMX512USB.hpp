/**
 * Eurolite DMX512 USB PRO 
 * wrapper with libusb
 * header-only library
 * Andrzej Kopeć, 2018
 */

#include <cstring>
#include <algorithm>
#include <mutex>
#include <string>
#include <sstream>
#include "libusb-1.0/libusb.h"

class LibUSB_EuroliteDMX512USB
{
  public:
    LibUSB_EuroliteDMX512USB();

    ~LibUSB_EuroliteDMX512USB();

    bool open_device();

    void close_device();

    /**
     * Sets all DMX channels to zero
     * (effectively blackout)
     */
    void clear_all_channels();

    /** Sets a value for a specified channel
     * 
     *  @param  channel Channel number (zero based)
     *  @param  value   Value for that channel (unsigned int)
     */
    void set_channel(int channel, unsigned char value);

    /** Sets values for consecuteve channels starting the first channel
     * 
     *  @param  c       Count of values
     *  @param  v       Pointer to values array (unsigned int)
     */
    void set_channel_array(int c, unsigned char *v);

    /** Sets values for consecuteve channels starting from selected channel
     * 
     *  @param  from    The first channel number to place values.
     *  @param  c       Count of values
     *  @param  v       Pointer to values array (unsigned int)
     * 
     */
    void set_channel_array_from(int from, int c, unsigned char *v);

    void set_channel_memcpy(int c, unsigned char *v);

    void set_channel_memcpy_from(int from, int c, unsigned char *v);

    // ---- SYNC TRANSFER ----
    /**
     * Transfer data in blocking (sync) mode.
     */
    void sync_transfer_data();

    // ---- ASYNC TRANSFER ----

    /**
     * Fill async transfer data struct and submit.
     */
    void async_transfer_fill_and_submit();

    /**
     * Cancel async transfer
     */
    void async_transfer_cancel();

    /**
     * A method calling libusb routine for handling events in async transfer.
     */
    void async_transfer_tick();

    /**
     * Method should be called regularily to service events generated from
     * async transfers. 
     */
    void service_async_transfer();

    /**
     * Enable or disable async transfer. 
     * The method creates transfer data struct and should take care of 
     * cancelling any pending submitted transfer and then remove
     * dealloc transfer data.
     * 
     */
    void enable_async_transfer(bool will_be_enabled);

    // ---- ACCESS THE STATE ----

    bool is_ready();

    bool get_async_transfer_enabled();

    void set_timeout(int atimeout);

    unsigned int get_timeout();

    std::string get_channels_as_string();

    const char *get_sync_transfer_status_name();

    const char *get_async_transfer_status_name();

    const char *get_async_submit_status_name();

    const char *get_async_event_status_name();

    // obsolete
    const char *get_sync_transmission_status_as_ccp();

  private:
    // --- device state

    // libusb context
    libusb_context *context = NULL;

    // a handle for USB device
    struct libusb_device_handle *euro_handle = NULL;

    // a flag indicating ready state of the device.
    // If true we can transmit/submit data
    bool ready = false;

    // timeout interval for response from USB device
    unsigned int timeout = 150u;

    // last sync transfer return status
    int sync_transfer_status = LIBUSB_SUCCESS;

    // --- DMX data

    // actual data (buffer) sent to USB device
    unsigned char *data;

    // the (constant) size of a data buffer
    static const size_t data_size = 518;

    // mutex on data buffer manipulation
    std::mutex data_mutex;

    // --- async transmission data

    // libusb_trasfer structure
    struct libusb_transfer *xfr = NULL;

    // timeout for handling libusb events // unused for now
    // struct timeval zero_tv = (struct timeval){ 0 };

    // a flag indicating if we can submit next transfer
    bool async_transfer_pending = false;

    // a flag indicating if process of async transmission is enabled
    bool async_transfer_enabled = false;

    // a flag indicating the state of trasition from enabled
    // to disabled (wait to cancel out pending submissions)
    bool async_transfer_wait_for_disable = false;

    // last async transfer result from callback
    libusb_transfer_status async_transfer_status = LIBUSB_TRANSFER_COMPLETED;

    // last async submit status
    int async_submit_status = LIBUSB_SUCCESS;

    // last event handling return status
    libusb_error async_event_handling_status = LIBUSB_SUCCESS;

    /**
     * A callback function for async transfer
     * as a user_data pointer to instance of class
     */
    static void cb_async_xfr_complete(struct libusb_transfer *x);

    /**
     * Initalize LibUSB
     */
    void initialize_libusb();

    /**
     * Initalize buffer data
     */
    void initialize_data();
};