from gi.repository import Gio, GLib
from gi._gi import variant_type_from_string
from gi.repository import Lia

PRIV_BUS_IFACE = \
  "<interface name=\"org.example.myapp.MyObj1Iface\"> \
     <method name=\"Ask\"> \
       <arg type=\"s\" name=\"question\" direction=\"in\"/> \
       <arg type=\"b\" name=\"answer\" direction=\"out\"/> \
     </method> \
   </interface>"

myApp = Lia.Application(service_name = "org.example.myapp1")

def on_method_called(app, bus_type, caller_id, obj_path, iface_name, method_name, args, inv_obj, user_data):
    msg = args.get_child_value(0)
    print(msg.get_string())

    vb = GLib.VariantBuilder()
    vb.init(variant_type_from_string('(b)'))
    vb.add_value(GLib.Variant("b", True))
    ret = vb.end()

    inv_obj.return_value(ret)

def on_register_objects(app, bus_type, user_data):
    app.register_object(bus_type,
                        "/org/example/myapp1/MyObj1",
                        bus_type,
                        on_method_called,
                        None,
                        None)

myApp.connect("register-objects", on_register_objects, None)

myApp.run()
