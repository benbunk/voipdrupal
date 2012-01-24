<?php

// $Id$

/**
 * @file
 * Implementation of the VoipServer-related classes
 */


/**
 * Class definitions
 */
        
abstract class VoipServer {

  private $id;  // internal server id
  private $name; // human-readable name of the server
  private $config_form; // (optional) name of form function for server configuration options
  private $config = NULL; // current configuration

  /*
   * Constructors
   */

  function __construct($id, $name, $config_form=NULL) {
    $this->id = $id;
    $this->name = $name;
    $this->config_form = $config_form;
  }

  public static function getServer($server_id) {
    $rc = NULL;
    $server_array = self::getServers();
    foreach($server_array as $server) {
      if($server->id == $server_id) {
        $rc = $server;
        break;
      }
    }
    return $rc;
  }

  public static function getDefaultServer() {
    $default_server_id = variable_get('voip_default_server', 'log');
    $default_server = self::getServer($default_server_id);
    return($default_server);
  }

  /*
   * Public methods
   */

  function getId() {
    return $this->id;
  }

  function getName() {
    return $this->name;
  }

  abstract function getDefaultLanguage();

  abstract function getDefaultVoice();

  function getConfigForm() {
    return $this->config_form;
  }

  function getConfig() {
    return $this->config;
  }

  public static function getServers() {
    $server_array = module_invoke_all('voipserver_info');
    foreach ($server_array as $server) {
      // retrieve server-specific configuration
      $server->config = variable_get('voip_'. $server->id .'_settings', '');
    }
    return $server_array;
  }


  /*
   * API methods
   */

  /**
   * Make a phone call
   * 
   * @param 
   * $voipcall, VoipCall instance with information about the call
   * 
   * @param 
   * $server_config, configuration of the voip server to be used for the call
   * 
   * @return
   * boolean, TRUE if success, FALSE in case of processing error
   */
  abstract function dial($voipcall, $server_config=NULL);
  
  /**
   * Send a text message
   * 
   * @param 
   * $text, body of the text message being sent
   *
   * @param 
   * $voipcall, VoipCall instance with the origin, destination, the script, and
   * addditional information about the way the message should be sent
   * 
   * @param 
   * $reply, whether the messages is to be sent as a reply to an already
   * existent VoipCall instance
   * 
   * @param 
   * $server_config, configuration of the VoIP server to be used
   * 
   * @return
   * boolean, TRUE if success, FALSE in case of processing error
   */
  abstract function send_text($text, $voipcall, $reply=FALSE, $server_config=NULL);

  /**
   * Hangup an existing call
   * 
   * @param 
   * $voipcall, VoipCall instance with information about the call to be hangup
   * 
   * @param 
   * $server_config, voip server to be used
   * 
   * @return
   * boolean, TRUE if success, FALSE in case of processing error
   */
  abstract function hangup($voipcall, $server_config=NULL);
  
  /**
   * Common entry point function for text message handling.
   * 
   * @param 
   * $text_content, with the text body
   * 
   * @param 
   * $origin, text origin
   *
   * @param 
   * $destination, text destination
   *
   * @param 
   * $network, text network
   *
   * @param 
   * $server_name, server name
   * 
   * @return
   * boolean, TRUE if success, FALSE in case of processing error
   */
  abstract function text_handler($text_content, $origin ,$destination, $time, $network, $server_name);


  /**
   * Test the connection with the voip server
   * 
   * @param 
   * $server_config, configuration of the voip server to be tested
   * 
   * @return
   * boolean, TRUE if success, FALSE in case of failure
   */
  abstract function ping($server_config=NULL);

}
