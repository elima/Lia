const Lia = imports.gi.Lia;
const GLib = imports.gi.GLib;

const PRIV_BUS_IFACE =
  "<interface name=\"org.example.myapp.MyObj1Iface\"> \
     <method name=\"Ask\"> \
       <arg type=\"s\" name=\"question\" direction=\"in\"/> \
       <arg type=\"b\" name=\"answer\" direction=\"out\"/> \
     </method> \
   </interface>";

let myApp = new Lia.Application ({
    "service-name": "org.example.myapp",
    "webview-html-root": "/home/elima/tmp/test/"
});

let onMethodCalled = function (app,
                               busType,
                               callerId,
                               objPath,
                               ifaceName,
                               methodName,
                               args,
                               invObj,
                               userData) {
    let vText = args.get_child_value (0);
    print (vText.get_string () [0]);
};

myApp.connect ("register-objects",
    function (app, busType) {
        app.register_object (busType,
                             "/org/example/myapp/MyObj1",
                             PRIV_BUS_IFACE,
                             onMethodCalled,
                             null,
                             null);
    });

myApp.run ();
