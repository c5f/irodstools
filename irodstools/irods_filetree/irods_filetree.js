// $Id$

// Wrap input on pageload.
jQuery(document).ready(function() {
  var input = jQuery('#field-irods-directory-values input');  

  jQuery(input).focus(function() {
    jQuery(this).attr('value', showPicker(this));
  });
});

// Function that fetches the json
function ajaxFetch() {
  var block = jQuery('.block-irods-filetree');
  jqxhr = jQuery.ajax({
    url      : '/irods_filetree',
    type     : 'get',
    dataType : 'json',
    xhrFields: {
      withCredentials: true
    },
    success  : function(data, textStatus, xhr) {
      var tree = data;
      output = outputJSON(tree);

      jQuery('.filetree', block).html(output);
      jQuery('.filetree', block).css('display', 'block');
      jQuery('.filetree_fetch_button', block).html('Refresh');
    },
    error    : function(xhr, textStatus, errorThrown) {
      output = "<p>An error occurred while connecting to iRODS:</p>";
      output += "<p>The status was: <strong>" + xhr['status'] + "</strong> " +
        errorThrown;
      console.log(xhr);

      jQuery('.filetree', block).html(output);
      jQuery('.filetree', block).css('display', 'block');
    }
  });
}

// Helper function that converts JSON into a nested list structure
function outputJSON(obj) {
  result = "<ul class=\"irods_parent\">";
  for (var key in obj) {
    result += "<li class=\"irods_node\" title=\"" + key + "\">"+ key;
    if (Object.keys(obj).length) {
      result += outputJSON(obj[key]);
    };
    result += "</li>";
  }
  result += "</ul>";

  return result;
}

/**
 * Shows the iRODS Filetree Picker.
 *
 * Uses element to attach the specific field to the picker so that the return 
 * value is assigned to the correct field.
 */
function showPicker(element) {
  var content = buildPicker(element);

  jQuery('body').append(content);
}

/**
 * Climbs up the directory structure to return the file path of a nested 
 * element.
 */
function getDirectory(element) {

  result = jQuery(element).attr('title');

  while (jQuery(element).parent().hasClass('irods_parent')) {
    element = jQuery(element).parent().parent();
    result = jQuery(element).attr('title') + "/" + result;
  }

  return result;
}

/**
 * Hides the iRODS Filetree Picker.
 */
function hidePicker() {

  // For overlay module
  if (location.href.indexOf('#overlay') >= 0) {
    jQuery('.overlay-active')
      .contents().find('.irods-filetree-picker-popup').remove();  
  };
  
  // For no iFrames
  jQuery('.irods-filetree-picker-popup').remove();
}

/**
 * Builds the iRODS Filetree Picker
 */
function buildPicker(element) {
  
  // The div that stores the picker content.
  var result = jQuery('<div>').addClass('irods-filetree-picker-popup');

  jQuery(result).append('<h2>iRODS File Picker®</h2>');
  jQuery(result).append('<ul>');
  jQuery('ul', result).addClass('irods-filetree-picker');
  jQuery('ul', result).append('<li>');
  jQuery('li', result).addClass('irods-filetree-picker options').html("/");

  jQuery.ajax({
    url      : '/irods_filetree',
    type     : 'get',
    dataType : 'json',
    xhrFields: {
      withCredentials: true
    },
    success  : function(data, textStatus, xhr) {
      var options = jQuery('.options', result);
      var tree = data;
      options.append(outputJSON(tree));
      jQuery('li', options).dblclick(function() {
        jQuery(element).attr('value', getDirectory(this));
        hidePicker();
      });
    },
    error    : function(xhr, textStatus, errorThrown) {
      output = "<p>An error occurred while connecting to iRODS:</p>";
      output += "<p>The status was: <strong>" + xhr['status'] + "</strong> " +
        errorThrown;
      console.log(xhr);
    }
  });
  
  return result;
}
