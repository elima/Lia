'use strict';

define ([
    "jQuery",
    "events",
    "promise",
    "navigator"
],
function ($, Events, Promise, Navigator) {
    var APPS_BASE_PATH = "../apps/";

    function inferUserId () {
        var pathTokens = location.pathname.split ("/");
        var name = pathTokens[1];

        var userId = "http://" + location.hostname + "/" + pathTokens[1];
        var userName = name + "@" + location.hostname;

        return userName;
    }

    function Shell () {
        var self = this;

        $.extend (Shell.prototype, new Events.EventEmitter ());

        var appsBySlug = {};
        var currentApp;

        this.createApp = function (moduleName) {
            var deferred = Promise.defer ();

            require ([moduleName],
                     function (App) {
                         var app = new App ({
                             dataPath: dataPath
                         });

                         appsBySlug[app.slug] = app;

                         deferred.resolve (app);
                     });

            return deferred.promise;
        };

        this.showApp = function (app) {
            if (currentApp) {
                // @TODO:
            }

            app.getView ("homeView", true)
                .then (function (widget) {
                           $ ("#shell-app-name-label")
                               .html (app.name)
                               .get (0).href = "#/" + app.slug;

/*
                           $ ("#content")
                               .append (widget.domNode)
                               .animate ({
                                   height: "toggle"
                               }, 400)
                               .show ();
*/
                           $ ("#content").append (widget.domNode).show ();

                           currentApp = app;

                           widget.show ();
                       },
                       function (error) {
                           console.log (error);
                       });
        };


        // load user session, if any
/*
        if (Account.loadSession ()) {
            // @TODO
        }
        else {
*/
            var userId = inferUserId ();
/*
            console.log ("No session, assuming user '" + userId + "'");
        }
*/


/*
        // @TODO: determine user data path
        var dataPath = "/elima/data";

        // @TODO: determine default application and launch it
        var appId = "net.free-social.apps.Blog";
        var appModuleName = APPS_BASE_PATH + appId + "/main";

        this.createApp (appModuleName)
            .then (function (app) {
                       self.showApp (app);
                   });
*/

        Navigator.start ();
        Navigator.on ("change",
                      function (oldAddress, address) {

                          address = (address.charAt (0) == "/" ? "" : "/") + address;

                          var tokens = address.split ("/");
                          var appSlug = tokens.length > 1 ? tokens[1] : null;

                          if (! appSlug)
                              return;

                          console.log (appSlug);
                          if (! currentApp || currentApp.slug != appSlug)
                              self.showApp (appsBySlug[appSlug]);
                      });
    }

    return Shell;
});
