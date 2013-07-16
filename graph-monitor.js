/*
 * Copyright 2013 Diomidis Spinellis
 *
 * Allow interactive monitoring of the process graph
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/* The URL used to periodically update the displayed popup */
var url;

/* The event that caused the popup to be displayed */
var popup_event = null;

/*
 * This is set by the hover handlers and used by the
 * asynchrounous JSON handlers to avoid a race condition.
 */
var popup_visibility;

/* The interval update used to update the popup */
var popup_update;

/* Process ids for each process node */
var node_pid = new Array();

function set_child_color(node, child_kind, color) {
	var child = node.getElementsByTagName(child_kind)[0];

	if (color == null) {
		child.setAttribute('stroke', child.oldStroke);
		child.setAttribute('fill', child.oldFill);
	} else {
		child.oldStroke = child.getAttribute('stroke')
		child.oldFill = child.getAttribute('fill')
		child.setAttribute('stroke', color);
		if (child_kind != "path")
			child.setAttribute('fill', color);
	}
}


/*
 * Format number with a comma as a thousand separator
 * See http://stackoverflow.com/a/2901298/20520
 */
function ts(x) {
    return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

/* Update the pipe popup box with the JSON result of the active URL */
update_pipe_content = function() {
	$.ajax({
                url: url,
                success: function(json) {
			$('#bytes').text(ts(json.nbytes));
			$('#lines').text(ts(json.nlines));
			$('#bps').text(ts((json.nbytes / json.rtime).toFixed(0)));
			$('#lps').text(ts((json.nlines / json.rtime).toFixed(0)));
			if (json.data.length > 500)
				json.data = json.data.substr(500) + "[...]";
			$('#record').text(json.data);

			var label = document.getElementById("pipePopup");
			label.style.visibility = popup_visibility;
			label.style.top = popup_event.pageY + 'px';
			label.style.left = (popup_event.pageX + 30) + 'px';
                },
		dataType: 'json',
		global: false
	});
}

/* Fill the process info box with the process data */
process_fill = function(data) {
	var table = document.getElementById("processData");
	$("#processData tr").remove();
	for (var i = 0; i < data.length; i++) {
		var process = data[i];
		if (process == null)
			break;
		var row = table.insertRow(-1);
		var cell = row.insertCell(0);
		cell.innerHTML = process["command"];
		var kv = process["kv"];
		for (var j = 0; j < kv.length; j++) {
			var row = table.insertRow(-1);
			var cell = row.insertCell(0);
			cell.className = 'colHead';
			cell.innerHTML = kv[j]['k'];
			var cell = row.insertCell(-1);
			cell.className = 'colValue';
			cell.innerHTML = kv[j]['v'];
		}
		if (i + 1 < data.length) {
			var row = table.insertRow(-1);
			var cell = row.insertCell(0);
			cell.innerHTML = '<hr />';
			cell.colSpan = 2;
		}
	}
}

/* Update the process popup box with the JSON result of the active URL */
update_process_content = function() {
	$.getJSON(
                url,
                {},
                function(json) {
			var label = document.getElementById("processPopup");
			process_fill(json);

			label.style.visibility = popup_visibility;
			label.style.top = popup_event.pageY + 'px';
			label.style.left = (popup_event.pageX + 30) + 'px';
                }
	);
}

over_edge_handler = function(e) {
	url = 'http://localhost:HTTP_PORT/mon-' + this.id;
	popup_event = e;
	popup_visibility = 'visible';
	update_pipe_content();
	popup_update = setInterval(update_pipe_content, 500);

	set_child_color(this, "path", "blue");
	set_child_color(this, "polygon", "blue");
}

out_edge_handler = function() {
	popup_visibility = 'hidden';
	var label = document.getElementById("pipePopup");
	label.style.visibility = 'hidden';
	clearInterval(popup_update);

	set_child_color(this, "path", null);
	set_child_color(this, "polygon", null);
}

over_store_handler = function(e) {
	// Remove leading "store:" prefix
	url = 'http://localhost:HTTP_PORT/mon-nps-' + this.id.substring(6);
	popup_event = e;
	popup_visibility = 'visible';
	update_pipe_content();
	popup_update = setInterval(update_pipe_content, 500);

	if (this.getElementsByTagName("ellipse")[0] != null)
		set_child_color(this, "ellipse", "blue");
	else
		set_child_color(this, "polygon", "blue");
}

out_store_handler = function(e) {
	popup_visibility = 'hidden';
	var label = document.getElementById("pipePopup");
	label.style.visibility = 'hidden';
	clearInterval(popup_update);

	set_child_color(this, "ellipse", null);
}

over_process_handler = function(e) {
	// Construct dynamic query
	url = 'http://localhost:HTTP_PORT/server-bin/rusage?pid=' + node_pid[this.id];
	popup_event = e;
	popup_visibility = 'visible';
	update_process_content();
	popup_update = setInterval(update_process_content, 3000);

	set_child_color(this, "ellipse", "blue");
}

out_process_handler = function(e) {
	popup_visibility = 'hidden';
	var label = document.getElementById("processPopup");
	label.style.visibility = 'hidden';
	clearInterval(popup_update);

	if (this.getElementsByTagName("ellipse")[0] != null)
		set_child_color(this, "ellipse", null);
	else
		set_child_color(this, "polygon", null);
}


/* Show a busy indicator during AJAX calls */
set_ajax_busy = function(e) {
	// Ajax activity indicator bound to ajax start/stop document events
	$(document).ajaxStart(function(){
		$('#ajaxBusy').show();
	}).ajaxStop(function(){
		$('#ajaxBusy').hide();
	});
}

$(document).ready(function() {
	var svg = document.getElementById('thesvg').getSVGDocument();
	if (!svg)
		svg = document.getElementById('thesvg');
	var all = svg.getElementsByTagName("g");

	set_ajax_busy();
	for (var i=0, max=all.length; i < max; i++) {
		var element = all[i];

		/* Set event handlers for edges and store nodes */
		className = element.className.baseVal;
		if (className.match(/edge.*/)) {
			$(element).hover(over_edge_handler, out_edge_handler);
		} else if (className.match(/node.*/)) {
			if (element.id.match(/store:/))
				$(element).hover(over_store_handler, out_store_handler);
			else {
				console.log("Get pid for " + element.id);

				/* Obtain node's pid and store it in node_pid */
				$.getJSON("pid-" + element.id + ".json", {},
					(function(nodeid) {
						return function(json) {
							node_pid[nodeid] = json.pid;
							console.log("pid of " + nodeid + " is " + json.pid);
						};
					}(element.id))
				);
				$(element).hover(over_process_handler, out_process_handler);
			}
		}

		/* Clear the title, which appears as a useless tooltip */
		var title = element.getElementsByTagName('title')[0];
		title.innerHTML = '';
	}
});
