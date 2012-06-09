"use strict";

define ([],
function () {

    function EventEmitter () {
        var listeners = [];

        this.addEventListener = function (event, handler) {
            if (! listeners[event])
                listeners[event] = [];

            listeners[event].push ({cb: handler, once: false});
        };
        this.addListener = this.on = this.addEventListener;

        this.once = function (event, handler) {
            if (! listeners[event])
                listeners[event] = [];

            listeners[event].push ({cb: handler, once: true});
        };

        function cleanupHandlers (event) {
            setTimeout (function () {
                var handlers = [];

                for (var i=0; i<listeners[event].length; i++)
                    if (listeners[event][i])
                        handlers.push (listeners[event][i]);

                listeners[event] = handlers;
            }, 0);
        }

        this.removeEventListener = function (event, handler) {
            if (! listeners[event])
                return;

            for (var i=0; i<listeners[event].length; i++)
                if (listeners[event][i].cb === handler)
                    listeners[event][i] = null;

            cleanupHandlers (event);
        };
        this.removeListener = this.removeEventListener;

        this.removeAllEventListeners = function (event) {
            if (arguments.length == 0)
                listeners = [];
            else
                delete (listeners[event.toString ()]);
        };
        this.removeAllListeners = this.removeAllEventListeners;

        this.emit = function () {
            var args = Array.prototype.slice.call (arguments);
            var event = args.shift ();

            if (! listeners[event])
                return;

            for (var i=0; i<listeners[event].length; i++)
                if (listeners[event][i]) {
                    var handler = listeners[event][i];

                    if (handler.once) {
                        listeners[event][i] = null;
                        cleanupHandlers (event);
                    }

                    handler.cb.apply (this, args);
                }
        };

        this.stealEmitter = function () {
            var emit = this.emit;
            delete (this.emit);

            return emit;
        };
    }

    return {
        EventEmitter: EventEmitter
    };
});
