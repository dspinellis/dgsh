var htmlDocument = document;

function get_tooltip_div(entity) {
     var name = entity.id;
     return document.getElementById('tooltip--' + name);
}

function set_child_color(node, child_kind, color) {
     var child = node.getElementsByTagName(child_kind)[0];

     if (color == null) {
         child.setAttribute('stroke', child.oldStroke);
         child.setAttribute('fill', child.oldFill);
     }
     else {
         child.oldStroke = child.getAttribute('stroke')
         child.oldFill = child.getAttribute('fill')
         child.setAttribute('stroke', color);
         if (child_kind != "path") {
      child.setAttribute('fill', color);
         }
     }
}

over_edge_handler = function(e) {
              var label = get_tooltip_div(this);
              label.style.visibility='visible';
              label.style.top=e.pageY + 'px';
              label.style.left=(e.pageX+30) + 'px';

              set_child_color(this, "path", "blue");
              set_child_color(this, "polygon", "blue");
        }

out_edge_handler = function() {
              var label = get_tooltip_div(this);
              label.style.visibility='hidden';

              set_child_color(this, "path", null);
              set_child_color(this, "polygon", null);
        }

over_node_handler = function(e) {
              var label = get_tooltip_div(this);
              label.style.visibility='hidden';

              if (label) {
              label.style.visibility='visible';
              label.style.top=e.pageY + 'px';
              label.style.left=(e.pageX+30) + 'px';
              }

              set_child_color(this, "ellipse", "blue");
          }

out_node_handler = function(e) {
              var label = get_tooltip_div(this);
              label.style.visibility='hidden';

              if (label) {
              label.style.visibility='hidden';
              }

              set_child_color(this, "ellipse", null);
         }

function onload() {
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
        }
        else if (className.match(/node.*/)) {
            element.onmouseover = over_node_handler;
            element.onmouseout = out_node_handler;
            ellipse = element.getElementsByTagName("ellipse")[0];
            if (ellipse.getAttribute('fill') == 'none') {
                ellipse.setAttribute('fill', 'white');
            }
        }
    }
}
