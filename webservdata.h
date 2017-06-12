/*
 * webservdata.h
 *
 *  Created on: Jun 12, 2017
 *      Author: Kevin
 */

#ifndef WEBSERVDATA_H_
#define WEBSERVDATA_H_



const char Header[] =
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/materialize/0.98.2/css/materialize.min.css\">"
"<link href=\"https://fonts.googleapis.com/icon?family=Material+Icons\" rel=\"stylesheet\">"
"<nav><div class=\"nav-wrapper teal\">"
"<a href=\"\" class=\"brand-logo\">Irrigation System</a>"
"<ul id=\"nav-mobile\" class=\"right hide-on-med-and-down\"></ul>"
"</div></nav>"
"<div class=\"card teal lighten-2\">"
"<div class=\"card-content\">"
"<span class=\"card-title\">System Info:</span>"
"<p>Current System Information</p>"
"</div>"
"<ul class=\"collection\">"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">import_export</i>"
"<a class=\"collection-item\"><span class=\"badge\">"
;

const char Header2[] =
"</span>MQTT</a>"
"</li>"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">developer_board</i>"
"<a href=\"http://"
;


const char Header21[] =
":1880\" class=\"collection-item\"><span class=\"badge\">"
;


const char Header3[] =
"</span>NodeRed</a>"
"</li>"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">blur_on</i>"
"<a class=\"collection-item\"><span class=\"badge\">"
"<a class=\"collection-item\">System Status</a>"
"<table class=\"striped\">"
"<tbody>"
"<tr><td>Current Temp</td><td>"
;

const char Header4[] =
"</td></tr>"
"<tr><td>Current Humidity</td><td>"
;

const char Body1[] =
"</td></tr>"
"</tbody>"
"</table>"
"</li>"
"</ul>"
"</div>"
"<div class=\"card teal lighten-2\">"
"<div class=\"card-content\">"
"<span class=\"card-title\">MQTT Topics</span>"
"<p>Current Topics being subscribed and published to</p>"
"</div>"
"<ul class=\"collection\">"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">file_download</i>"
"<table class=\"striped\">"
"<thead>"
"<tr><th>Subscriptions</th></tr>"
"</thead>"
"<tbody>"
"<tr><td>"
;

const char Table1[] =
"</td></tr>"
"<tr><td>"
;

const char Body2[] =
"</td></tr>"
"</tbody>"
"</table>"
"</li>"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">file_upload</i>"
"<table class=\"striped\">"
"<thead>"
"<tr><th>Publications</th></tr>"
"</thead>"
"<tbody>"
"<tr><td>"
;

const char Body3[] =
"</td></tr>"
"</tbody>"
"</table>"
"</li>"
"</ul>"
"</div>"
;















#endif /* WEBSERVDATA_H_ */
