<?php

// $Id$

/**
 * @file
 * Implementation of the VoipLogServer class
 */


/**
 * Class definitions
 */

class VoipLogServer extends VoipServer {


  /*
   * Constructors
   */

  function __construct() {
    $id = 'log';
    $name = t('Log only');
    parent::__construct($id, $name);
  }


  /*
   * Public methods
   *
   * NOTE: the following methods are not to be called. They are only implemented
   *       because VoipServer is an abstract class.
   */

  function getDefaultLanguage() {
    return 'en';
  }

  function getDefaultVoice() {
    return 'man';
  }


  /*
   * API methods
   */

  function dial($voipcall, $server_config = NULL) {
    $params = array(
      'voipcall' => $voipcall,
      'options' => $options,
      'server_config' => $server_config,
    );
    watchdog('voip', 'VoipLogServer dial() called with: @params', array('@params' => print_r($params, TRUE)), WATCHDOG_INFO);
    return TRUE;
  }
  function send_text($text, $voipcall, $reply = FALSE, $server_config = NULL) {
    $params = array(
      'text' => $text,
      'voipcall' => $voipcall,
      'reply' => $reply,
      'options' => $options,
      'server_config' => $server_config,
    );
    watchdog('voip', 'VoipLogServer send_text() called with: @params', array('@params' => print_r($params, TRUE)), WATCHDOG_INFO);
    return TRUE;
  }

  function hangup($voipcall, $server_config = NULL) {
    $params = array(
      'voipcall' => $voipcall,
      'server_config' => $server_config,
    );
    watchdog('voip', 'VoipLogServer hangup() called with: @params', array('@params' => print_r($params, TRUE)), WATCHDOG_INFO);
    return TRUE;
  }

  function text_handler($text_content, $origin, $destination, $time, $network, $server_name) {
    $params = array(
      'text_content' => $text_content,
      'origin' => $origin,
      'destination' => $destination,
      'time' => $time,
      'network' => $network,
      'server_name' => $server_name,
    );
    watchdog('voip', 'VoipLogServer text_handler() called with: @params', array('@params' => print_r($params, TRUE)), WATCHDOG_INFO);
    return TRUE;
  }

  function ping($server_config = NULL) {
    $options = array('server_config' => $server_config);
    watchdog('voip', 'VoipLogServer ping() called with: @options', array('@options' => print_r($options, TRUE)), WATCHDOG_INFO);
    return TRUE;
  }

}
