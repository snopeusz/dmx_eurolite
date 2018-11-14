#include "libUSB_EuroliteDMX512USB.hpp"


#define print_debug(xxx)

LibUSB_EuroliteDMX512USB::LibUSB_EuroliteDMX512USB()
    : data(new unsigned char[data_size]), timeout(150u)
{
    initialize_data();
    initialize_libusb();
}

LibUSB_EuroliteDMX512USB::~LibUSB_EuroliteDMX512USB()
{
    if (ready)
        close_device();

    if (context != NULL)
        libusb_exit(context);

    delete[] data;
}

void LibUSB_EuroliteDMX512USB::initialize_libusb()
{
    int ret = libusb_init(&context);
    if (ret < 0)
        context = NULL;
    return;
}

void LibUSB_EuroliteDMX512USB::initialize_data()
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        data[0] = 0x7e;                // start of message
        data[1] = 0x06;                // dmx label
        data[2] = 0b00000001;          // universe size LSB
        data[3] = 0b00000010;          // universe size MSB
        data[4] = 0;                   // start code (Null Start Code)
        std::memset(data + 5, 0, 512); // zero whole DMX univ.
        data[data_size - 1] = 0xe7;    // end of message
    }

// Report state
bool LibUSB_EuroliteDMX512USB::is_ready()
{
    return ready;
}

bool LibUSB_EuroliteDMX512USB::open_device()
{
    print_debug("Opening device");
    if (context == NULL)
        initialize_libusb();
    if (context != NULL)
    {
        if (euro_handle == NULL)
            euro_handle = libusb_open_device_with_vid_pid(context, 0x4d8, 0xfa63);
        if (euro_handle != NULL)
        {
            print_debug("Device opened.");
            int ret = libusb_claim_interface(euro_handle, 1);
            if (ret >= 0)
            {
                print_debug("Interface claimed.");
                // async - on
                enable_async_transfer(true);
                ready = true;
                return ready;
            }
        }
    }
    ready = false;
    return ready;
}

void LibUSB_EuroliteDMX512USB::close_device()
{
    print_debug("Closing device.");
    if (euro_handle != NULL)
    {
        int ret = libusb_release_interface(euro_handle, 1);
        if (ret >= 0)
        {
            // async - off
            enable_async_transfer(false);
            libusb_close(euro_handle);
            euro_handle = NULL;
            ready = false;
        }
    }
}

/**
     * Methods for setting DMX buffer data
     */

void LibUSB_EuroliteDMX512USB::clear_all_channels()
{
    data_mutex.lock();
    std::memset(data + 5, 0, 512);
    data_mutex.unlock();
}

/** Sets a value for a specified channel
     * 
     *  @param  channel Channel number (zero based)
     *  @param  value   Value for that channel (unsigned int)
     */
void LibUSB_EuroliteDMX512USB::set_channel(int channel, unsigned char value)
{
    if (channel >= 0 && channel < 512)
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        data[channel + 5] = value;
    }
}

/** Sets values for consecuteve channels starting the first channel
     * 
     *  @param  c       Count of values
     *  @param  v       Pointer to values array (unsigned int)
     */
void LibUSB_EuroliteDMX512USB::set_channel_array(int c, unsigned char *v)
{
    if (c > 512)
        c = 512;
    std::lock_guard<std::mutex> lock(data_mutex);
    for (c--; c >= 0; c--)
        data[5 + c] = v[c];
}

/** Sets values for consecuteve channels starting from selected channel
     * 
     *  @param  from    The first channel number to place values.
     *  @param  c       Count of values
     *  @param  v       Pointer to values array (unsigned int)
     * 
     */
void LibUSB_EuroliteDMX512USB::set_channel_array_from(int from, int c, unsigned char *v)
{
    if (from > 511) // safety check
        return;
    if (c + from > 512) // safety check
        c = 512 - from;
    std::lock_guard<std::mutex> lock(data_mutex);
    for (c--; c >= 0; c--)
        data[from + 5 + c] = v[c];
}

void LibUSB_EuroliteDMX512USB::set_channel_memcpy(int c, unsigned char *v)
{
    if (c > 512)
        c = 512;
    std::lock_guard<std::mutex> lock(data_mutex);
    std::memcpy(data + 5, v, c);
}

void LibUSB_EuroliteDMX512USB::set_channel_memcpy_from(int from, int c, unsigned char *v)
{
    if (from > 511) // safety check
        return;
    if (c + from > 512) // safety check
        c = 512 - from;
    std::lock_guard<std::mutex> lock(data_mutex);
    std::memcpy(data + 5 + from, v, c);
}

// ---- SYNC TRANSFER ----
/**
     * Transfer data in blocking (sync) mode.
     */
void LibUSB_EuroliteDMX512USB::sync_transfer_data()
{
    if (!ready)
        return;
    print_debug("sync transfer stared");
    data_mutex.lock();
    // let's assume the interface has a proper EP (0x02), interface #1
    sync_transfer_status = libusb_bulk_transfer(
        euro_handle,                 // dev handle
        (0x2 | LIBUSB_ENDPOINT_OUT), // EP
        data,                        // data
        data_size,                   // size
        NULL,                        // & bytes sent
        timeout                      // timeout (ms)
    );
    data_mutex.unlock();
    if (sync_transfer_status == LIBUSB_ERROR_NO_DEVICE)
        close_device();
    print_debug("sync transfer completed");
}

// ---- ASYNC TRANSFER ----

/**
     * Fill async transfer data struct and submit.
     */
void LibUSB_EuroliteDMX512USB::async_transfer_fill_and_submit()
{
    // device must be ready! (tested in "service_async_transfer" method)
    if (async_transfer_pending)
        return;
    print_debug("async transfer fill&submit starts");
    data_mutex.lock();
    libusb_fill_bulk_transfer(
        xfr,
        euro_handle,                 // dev handle
        (0x2 | LIBUSB_ENDPOINT_OUT), // EP (OUT 0x2, interface #1)
        data,                        // data
        data_size,                   // size
        cb_async_xfr_complete,       // callback
        this,                        // user_data = this instance
        timeout                      // timeout (ms)
    );
    async_submit_status = libusb_submit_transfer(xfr);
    data_mutex.unlock();
    print_debug("async transfer fill&submit finished");

    if (async_submit_status == LIBUSB_ERROR_NO_DEVICE)
        close_device();
    else if (async_submit_status == LIBUSB_SUCCESS)
        async_transfer_pending = true;
}

/**
     * Cancel async transfer
     */
void LibUSB_EuroliteDMX512USB::async_transfer_cancel()
{
    if (async_transfer_pending) // cancel only if there is something to cancel
        libusb_cancel_transfer(xfr);
}

/**
     * A method calling libusb routine for handling events in async transfer.
     */
void LibUSB_EuroliteDMX512USB::async_transfer_tick()
{
    //libusb_handle_events_timeout_completed(context, &zero_tv, NULL);
    async_event_handling_status = (libusb_error)libusb_handle_events_completed(context, NULL);
}

/**
     * Method should be called regularily to service events generated from
     * async transfers. 
     */
void LibUSB_EuroliteDMX512USB::service_async_transfer()
{
    async_transfer_tick();
    if (!ready)
        return;
    if (async_transfer_enabled && !async_transfer_wait_for_disable)
        async_transfer_fill_and_submit();
    else
    {
        if (async_transfer_pending)
        {
            async_transfer_cancel();
        }
    }
}

/**
     * Enable or disable async transfer. 
     * The method creates transfer data struct and should take care of 
     * cancelling any pending submitted transfer and then remove
     * dealloc transfer data.
     * 
     */
void LibUSB_EuroliteDMX512USB::enable_async_transfer(bool will_be_enabled)
{
    if (will_be_enabled && !async_transfer_enabled)
    {
        // allocate and initialize async transfer data
        if (!xfr)
            xfr = libusb_alloc_transfer(0);
        async_transfer_enabled = true;
    }
    else if (!will_be_enabled && async_transfer_enabled)
    {
        if (async_transfer_pending)
        {
            async_transfer_cancel();
            async_transfer_wait_for_disable = true;
            async_transfer_tick(); // probably we need to check until transfer is cancelled?
        }
        if (!async_transfer_pending && !async_transfer_wait_for_disable)
        {
            async_transfer_enabled = false;
            if (xfr)
                libusb_free_transfer(xfr);
            xfr = NULL;
        }
    }
}

// ---- ACCESS THE STATE ----

bool LibUSB_EuroliteDMX512USB::get_async_transfer_enabled()
{
    return async_transfer_enabled;
}

void LibUSB_EuroliteDMX512USB::set_timeout(int atimeout)
{
    // if (atimeout < 25)
    //     timeout = 25u;
    // else
    timeout = (unsigned int)atimeout;
}

unsigned int LibUSB_EuroliteDMX512USB::get_timeout()
{
    return timeout;
}

std::string LibUSB_EuroliteDMX512USB::get_channels_as_string()
{
    std::ostringstream oss;
    for (int i = 0; i < 512; i++)
    {
        oss << std::hex << (int)data[i + 5] << " ";
    }
    return oss.str();
}

const char *LibUSB_EuroliteDMX512USB::get_sync_transfer_status_name()
{
    return libusb_error_name(sync_transfer_status);
}

const char *LibUSB_EuroliteDMX512USB::get_async_transfer_status_name()
{
    return libusb_error_name(async_transfer_status);
}

const char *LibUSB_EuroliteDMX512USB::get_async_submit_status_name()
{
    return libusb_error_name(async_submit_status);
}

const char *LibUSB_EuroliteDMX512USB::get_async_event_status_name()
{
    return libusb_error_name(async_event_handling_status);
}

void LibUSB_EuroliteDMX512USB::cb_async_xfr_complete(struct libusb_transfer *x)
{
    LibUSB_EuroliteDMX512USB *me = (LibUSB_EuroliteDMX512USB *)x->user_data;
    me->async_transfer_pending = false;
    me->async_transfer_status = x->status;
    if (x->status == LIBUSB_TRANSFER_NO_DEVICE)
        me->close_device();
    else if (x->status == LIBUSB_TRANSFER_CANCELLED && me->async_transfer_wait_for_disable)
    {
        me->async_transfer_wait_for_disable = false;
        me->enable_async_transfer(false);
    }
    // switch(x->status) {
    //     case LIBUSB_TRANSFER_COMPLETED:
    //         break;
    //     case LIBUSB_TRANSFER_CANCELLED:
    //     case LIBUSB_TRANSFER_NO_DEVICE:
    //     case LIBUSB_TRANSFER_TIMED_OUT:
    //     case LIBUSB_TRANSFER_ERROR:
    //     case LIBUSB_TRANSFER_STALL:
    //     case LIBUSB_TRANSFER_OVERFLOW:
    //         // Various type of errors here
    //         break;
    // }
}
