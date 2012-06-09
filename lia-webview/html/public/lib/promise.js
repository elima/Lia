'use strict';

define (["./events"],
function (Events) {

    function Promise () {
        var self = this;

        var value;
        var error;
        var resolved = false;

        var listeners = [];

        this.resolve = function (_value) {
            if (resolved)
                throw (new Error ("Promise has already been resolved"));

            value = _value;
            resolved = true;

            for (var i in listeners)
                if (listeners[i].callback)
                    listeners[i].callback.apply (this, [value]);
        };

        this.reject = function (_error) {
            if (resolved)
                throw (new Error ("Promise has already been rejected"));

            error = _error;
            resolved = true;

            for (var i in listeners)
                if (listeners[i].errback)
                    listeners[i].errback.apply (this, [error]);
            listeners = undefined;
        };

        this.progress = function (_progress) {
            if (resolved)
                throw (new Error ("Promise has already been resolved"));

            var progress = _progress;

            for (i in listeners)
                if (listeners[i].progback)
                    listeners[i].progback.apply (this, [progress]);
        };

        this.then = function (callback, errback, progback) {
            if (value !== undefined) {
                // Promise has already being fullfilled,
                // call callback in the next main-loop turn
                if (callback)
                    setTimeout (function () {
                                    callback.apply (self, [value]);
                                }, 0);
            }
            else if (error !== undefined) {
                // Promise has already been rejected
                // call errback in the next main-loop turn
                if (errback)
                    setTimeout (function () {
                                    errback.apply (self, [error]);
                                }, 0);
            }
            else {
                // save callbacks
                listeners.push ({
                    callback: callback,
                    errback: errback,
                    progback: progback
                });
            }
        };
    }

    function Deferred () {
        var promise = new Promise ();

        var resolveFunc = promise.resolve;
        promise.resolve = function () {
            throw (new Error ("Not authorized to resolve this promise"));
        };

        var rejectFunc = promise.reject;
        promise.reject = function () {
            throw (new Error ("Not authorized to reject this promise"));
        };

        var progressFunc = promise.progress;
        promise.progress = function () {
            throw (new Error ("Not authorized to progress this promise"));
        };

        this.resolve = function () {
            resolveFunc.apply (promise, arguments);
        };

        this.reject = function () {
            rejectFunc.apply (promise, arguments);
        };

        this.progress = function () {
            progressFunc.apply (promise, arguments);
        };

        this.__defineGetter__ ("promise",
            function () {
                return promise;
            });
    }

    function defer () {
        return new Deferred ();
    };

    function isPromise (obj) {
        return obj && typeof (obj) == "object" && obj.constructor == Promise;
    }

    function when () {
        var d = defer ();

        var resolved = 0;
        var total = arguments.length;
        var result = [];
        var promises = arguments;

        function completed (index, value) {
            result[index] = value;

            if (++resolved == total)
                d.resolve (result);
        }

        for (var i=0; i<promises.length; i++)
            if (isPromise (promises[i])) {
                var p = promises[i];

                p.then ((function (index) {
                             return function (value) {
                                 completed (index, value);
                             };
                         }) (i),
                        function (err) {
                            d.reject (err);
                            i = total;
                        });
            }
            else {
                result[i] = promises[i];

                if (++resolved == total)
                    d.resolve (result);
            }

        return d.promise;
    }

    function Cancellable () {
        $.extend (Cancellable.prototype, new Events.EventEmitter ());
        var emit = Cancellable.prototype.stealEmitter ();

        var cancelled = false;

        this.cancel = function (reason) {
            if (cancelled)
                return;

            cancelled = true;
            emit ("cancelled", reason);
        };

        this.reset = function () {
            cancelled = false;
        };
    }

    return {
        Promise: Promise,
        Deferred: Deferred,
        defer: defer,
        isPromise: isPromise,
        when: when,
        Cancellable: Cancellable
    };
});
