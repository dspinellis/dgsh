var htmlDocument = document;

function get_popup_info_div(id) {
	return document.getElementById(id);
}

function set_child_color(node, child_kind, color) {
	var child = node.getElementsByTagName(child_kind)[0];

	if (color == null) {
		child.setAttribute('stroke', child.oldStroke);
		child.setAttribute('fill', child.oldFill);
	} else {
		child.oldStroke = child.getAttribute('stroke')
		child.oldFill = child.getAttribute('fill')
		child.setAttribute('stroke', color);
		if (child_kind != "path") {
			child.setAttribute('fill', color);
		}
	}
}

function clear_child_title(node, child_kind) {
	var child = node.getElementsByTagName(child_kind)[0];

	child.setAttribute('title', "");
}

/* Update the popup e with the JSON result of the URL */
update_content = function(url, e) {
	$.getJSON(
                url,
                {},
                function(json) {
			$('#bytes').text(json.nbytes);
			$('#lines').text(json.nlines);
			$('#bps').text((json.nbytes / json.rtime).toFixed(0));
			$('#lps').text((json.nlines / json.rtime).toFixed(0));
			$('#record').text(json.data);

			var label = get_popup_info_div("edge");
			label.style.visibility='visible';
			label.style.top=e.pageY + 'px';
			label.style.left=(e.pageX+30) + 'px';
                }
	);
}

over_edge_handler = function(e) {
	var url = 'http://localhost:HTTP_PORT/mon-' + this.id;

	update_content(url, e);

	set_child_color(this, "path", "blue");
	set_child_color(this, "polygon", "blue");
	clear_child_title(this, "path");
	clear_child_title(this, "polygon");
}

out_edge_handler = function() {
	var label = get_popup_info_div("edge");
	label.style.visibility='hidden';

	set_child_color(this, "path", null);
	set_child_color(this, "polygon", null);
}

over_node_handler = function(e) {
	// Remove leading "store:" prefix
	var url = 'http://localhost:HTTP_PORT/mon-nps-' + this.id.substring(6);

	//console.log(url);
	update_content(url, e);

	if (this.getElementsByTagName("ellipse")[0] != null) {
		set_child_color(this, "ellipse", "blue");
		clear_child_title(this, "ellipse");
	} else {
		set_child_color(this, "polygon", "blue");
		clear_child_title(this, "polygon");
	}
}

out_node_handler = function(e) {
	var label = get_popup_info_div("edge");
	label.style.visibility='hidden';

	if (label) {
		label.style.visibility='hidden';
	}

	if (this.getElementsByTagName("ellipse")[0] != null) {
		set_child_color(this, "ellipse", null);
	} else {
		set_child_color(this, "polygon", null);
	}
}

$(document).ready(function() {
	var svg = document.getElementById('thesvg').getSVGDocument();
	if (!svg) {
		svg = document.getElementById('thesvg');
	}
	var all = svg.getElementsByTagName("g");

	for (var i=0, max=all.length; i < max; i++) {
		var element = all[i];
		className = element.className.baseVal;
		if (className.match(/edge.*/)) {
			element.onmouseover = over_edge_handler;
			element.onmouseout = out_edge_handler;
		} else if (className.match(/node.*/) && element.id.match(/store:/)) {
			element.onmouseover = over_node_handler;
			element.onmouseout = out_node_handler;
		}
	}
});
