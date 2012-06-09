'use strict';

define ([
    "jQuery",
    "./events"
],
function ($, Events) {

    // Fragment-ID based navigation
    return new (function () {
        $.extend (this.constructor.prototype, new Events.EventEmitter ());
        var emit = this.constructor.prototype.stealEmitter ();

        var _interval = 50;
        var _currentFragid = null;

        var self = this;
        var _checkFunc = function () {
            check ();
        };

        var _intervalId = null;

        this.__defineGetter__ ("address", function () {
                                   return _currentFragid;
                               });

        function _getFragmentId () {
            var hash = window.location.hash;
            if (hash)
                hash = hash.substr (1);
            return hash;
        }

        function check () {
            var newFragid = _getFragmentId ();
            if (newFragid != _currentFragid) {
                var oldFragid = _currentFragid;
                _currentFragid = newFragid;
                emit ("change", oldFragid, newFragid);
            }
        }

        this.start = function () {
            if (! _intervalId) {
                _intervalId = window.setInterval (_checkFunc, _interval);
                check ();
            }
        };

        this.stop = function () {
            if (_intervalId) {
                window.removeInterval (_intervalId);
                _currentFragid = null;
            }
        };

        this.navigateTo = function (fragId) {
            window.location.hash = fragId;
        };

        this.getFragid = function () {
            return _currentFragid;
        };
    }) ();
});
