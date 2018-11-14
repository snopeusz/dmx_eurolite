/**
        @file
        dmx_eurolite - simple interface for Eurolite USB DMX512 PRO device.
        Based on libusb library.

        @ingroup	Comm
*/

#include <string>
#include <vector>
#include "c74_max.h"
#include "libUSB_EuroliteDMX512USB.hpp"

using namespace c74::max;

struct t_dmx_eurolite {
  t_object ob;
  LibUSB_EuroliteDMX512USB *dmx;
  t_qelem* sync_qelem;
  t_qelem* async_qelem;
  t_qelem* test_qelem; //TODO:DELME
};

static t_class *this_class = nullptr;

// QElem tasks
void sync_transfer_qtask(t_dmx_eurolite *self) {
	self->dmx->sync_transfer_data();
}

void async_transfer_qtask(t_dmx_eurolite *self) {
	self->dmx->service_async_transfer();
}

// TODO:DELME
void test_qtask(t_dmx_eurolite *self) {
	object_post((t_object*)self, "QTask run form Qelem.");
}


void *dmx_eurolite_new(t_symbol *name, long argc, t_atom *argv) {
  t_dmx_eurolite *self = (t_dmx_eurolite *)object_alloc(this_class);

  self->dmx = new LibUSB_EuroliteDMX512USB();
  self->dmx->set_timeout(150);

  attr_args_process(self, argc, argv);

  self->sync_qelem = qelem_new(self, (method)sync_transfer_qtask);
  self->async_qelem = qelem_new(self, (method)async_transfer_qtask);
  self->test_qelem = qelem_new(self, (method)test_qtask); // TODO:DELME

  return self;
}

void dmx_eurolite_free(t_dmx_eurolite *self) { 
	qelem_free(self->sync_qelem);
	qelem_free(self->async_qelem);
	qelem_free(self->test_qelem); // TODO:DELME
	delete self->dmx; 
}

//void qelem_set(t_qelem);

void dmx_eurolite_bang(t_dmx_eurolite *self) {
  //self->dmx->sync_transfer_data();
  	qelem_set(self->test_qelem); // TODO:DELME
}


void dmx_eurolite_sync(t_dmx_eurolite *self) {
  qelem_set(self->sync_qelem); 
}

void dmx_eurolite_async(t_dmx_eurolite *self) {
  qelem_set(self->async_qelem);
}

void dmx_eurolite_open(t_dmx_eurolite *self) { self->dmx->open_device(); }

void dmx_eurolite_close(t_dmx_eurolite *self) { self->dmx->close_device(); }

void dmx_eurolite_clear(t_dmx_eurolite *self) {
  self->dmx->clear_all_channels();
}

void dmx_eurolite_postinfo(t_dmx_eurolite *self) {
  object_post((t_object *)self,
              "Device is %s ready. \n"
			  "SYNC transfer status: %s\n" 
			  "ASYNC enabled: %s\n"
			  "ASYNC submit status: %s\n"
			  "ASYNC trasfer(cb) status: %s\n"
			  "ASYNC event handling status: %s\n"
			  "DMX buffer: %s \n",
              (self->dmx->is_ready() ? "" : "not"),
              //self->dmx->get_sync_transmission_status_as_ccp(),
			  self->dmx->get_sync_transfer_status_name(),
			  (self->dmx->get_async_transfer_enabled() ? "YES" : "NO"),
			  self->dmx->get_async_submit_status_name(),
			  self->dmx->get_async_transfer_status_name(),
			  self->dmx->get_async_event_status_name(),
              self->dmx->get_channels_as_string().c_str());
}

void dmx_eurolite_setchannel(t_dmx_eurolite *self, long ch, long val) {
  self->dmx->set_channel(
      ch, static_cast<unsigned char>(std::max(0l, std::min(255l, val))));
}
/**
 *
 * This method goes along "set" int standart [table] object:
 * set <ch_num> <val1> [<val2>...]
 */
void dmx_eurolite_set(t_dmx_eurolite *self, t_symbol *sym, long argc,
                      t_atom *argv) {
  if (argc < 2) return;
  const int first = atom_getlong(argv);  // first channel from to copy
  const int data_count = argc - 1;
  std::vector<unsigned char> data;
  data.reserve(data_count);
  // iterate from the second element of argv to the end
  for (int i = 1; i < argc; i++) {
    data.push_back(
        static_cast<unsigned char>(clamp((int)atom_getlong(argv + i), 0, 255)));
  }
  self->dmx->set_channel_array_from(first, data_count, data.data());
}

// TODO: REMOVE
// void dmx_eurolite_setrgb1(t_dmx_eurolite *self, long ir, long ig, long ib) {
//   unsigned char r =
//       static_cast<unsigned char>(std::max(0l, std::min(255l, ir)));
//   unsigned char g =
//       static_cast<unsigned char>(std::max(0l, std::min(255l, ig)));
//   unsigned char b =
//       static_cast<unsigned char>(std::max(0l, std::min(255l, ib)));
//   self->dmx->set_rgb_from(0, r, g, b);
// }

void dmx_eurolite_assist(t_dmx_eurolite *self, void *unused,
                         t_assist_function io, long index, char *string_dest) {
  if (io == ASSIST_INLET)
    strncpy(string_dest, "Message In", ASSIST_STRING_MAXSIZE);
}

// -- ATTRIBUTES

t_max_err dmx_eurolite_timeout_set(t_dmx_eurolite *x, t_object *attr, long argc,
                                   t_atom *argv) {
  long to = clamp((long)atom_getlong(argv), 20L, 2500L);
  x->dmx->set_timeout(to);
  return 0;
}

t_max_err dmx_eurolite_timeout_get(t_dmx_eurolite *x, t_object *attr,
                                   long *argc, t_atom **argv) {
  char alloc;
  atom_alloc(argc, argv, &alloc);
  atom_setlong(*argv, x->dmx->get_timeout());
  return 0;
}

t_max_err dmx_eurolite_ready_get(t_dmx_eurolite *x, t_object *attr, long *argc,
                                 t_atom **argv) {
  char alloc;
  atom_alloc(argc, argv, &alloc);
  atom_setlong(*argv, (x->dmx->is_ready() ? 1L : 0L));
  return 0;
}

t_max_err dmx_eurolite_ready_set(t_dmx_eurolite *x, t_object *attr, long argc,
                                 t_atom *argv) {
  return 0;  // DOES NOTHING
}

void ext_main(void *r) {
  this_class = class_new("dmx.eurolite", (method)dmx_eurolite_new,
                         (method)dmx_eurolite_free, sizeof(t_dmx_eurolite), 0L,
                         A_GIMME,
                         0);

  class_addmethod(this_class, (method)dmx_eurolite_bang, "bang", 0);
  class_addmethod(this_class, (method)dmx_eurolite_sync, "sync", 0);
  class_addmethod(this_class, (method)dmx_eurolite_sync, "async", 0);


  class_addmethod(this_class, (method)dmx_eurolite_setchannel, "setchannel",
                  A_DEFLONG, A_DEFLONG, 0);
  class_addmethod(this_class, (method)dmx_eurolite_set, "set", A_GIMME, 0);
  class_addmethod(this_class, (method)dmx_eurolite_open, "open", 0);
  class_addmethod(this_class, (method)dmx_eurolite_close, "close", 0);
  class_addmethod(this_class, (method)dmx_eurolite_clear, "clear", 0);
  class_addmethod(this_class, (method)dmx_eurolite_postinfo, "postinfo", 0);
  class_addmethod(this_class, (method)dmx_eurolite_assist, "assist", A_CANT, 0);

  CLASS_ATTR_LONG(this_class, "timeout", 0, t_dmx_eurolite, ob);
  CLASS_ATTR_LABEL(this_class, "timeout", 0, "Transfer timeout (ms)");
  // does it change anything?	actual limiting happens in accessors
  CLASS_ATTR_MAX(this_class, "timeout", 0, "2500");
  CLASS_ATTR_MIN(this_class, "timeout", 0, "25");
  CLASS_ATTR_ACCESSORS(this_class, "timeout", dmx_eurolite_timeout_get,
                       dmx_eurolite_timeout_set);

  // readonly attribute with custom no-op setter (querying the state of dmx
  // object)
  CLASS_ATTR_LONG(this_class, "ready", ATTR_SET_OPAQUE, t_dmx_eurolite, ob);
  CLASS_ATTR_STYLE_LABEL(this_class, "ready", 0, "onoff", "Device ready state");
  CLASS_ATTR_ACCESSORS(this_class, "ready", dmx_eurolite_ready_get,
                       dmx_eurolite_ready_set);

  // FIXME: only for quick testing, remove later:
  // class_addmethod(this_class, (method)dmx_eurolite_setrgb1, "setrgb1",
  //                 A_DEFLONG, A_DEFLONG, A_DEFLONG, 0);

  class_register(CLASS_BOX, this_class);
}
