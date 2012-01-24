<?php
// $Id$

/**
 * @file
 * Provides error handling support for system failures associated with Voip Drupal
 */

/**
 * Global variables
 */

// Stores the current error object
$_voip_error;

/*
 * Report processing errors associated with the Voip Drupal infrastructure.
 */
function voip_error($message = NULL) {
  global $_voip_error;
  if ($message) {
    $_voip_error = new stdClass();
    $_voip_error->is_error = TRUE;
    $_voip_error->message = $message;
  }
  return $_voip_error;
}

function voip_error_message() {
  global $_voip_error;
  return $_voip_error->message;
}

function voip_error_reset() {
  global $_voip_error;
  $_voip_error = NULL;
}

