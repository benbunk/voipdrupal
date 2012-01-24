<?php

// $Id$

/**
 * @file
 * Implementation of the VoipPlivoServer class
 */


/**
 * File dependencies
 */

foreach (array('plivo.php', 'voipplivo.inc') as $file) {
  require_once(dirname(__FILE__) . DIRECTORY_SEPARATOR . $file);
}

module_load_include('inc', 'voip', 'includes/voipserver');
module_load_include('inc', 'voipcall', 'voipcall');


/**
 * Class definitions
 */
        
class VoipPlivoServer extends VoipServer {

  /*
   * Constructors
   */

  function __construct() {
    $id = 'plivo';
    $name = 'Plivo';
    $config_form = 'voipplivo_admin_form';
    parent::__construct($id, $name, $config_form);
  }


  /*
   * Public methods
   */

  static function getVoicesList(){
    $voices_available = self::_get_available_voices();
    $list = array();
    foreach($voices_available as $id => $voice) {
      $list[$id] = $voice['description'];
    }
    asort($list);
    return $list;
  }

  /**
   * Returns the Plivo id associated with the given $voice and $language
   */
  static function getVoiceId($voice, $language) {
    $voices = VoipTropoServer::_get_available_voices();
    foreach ($voices as $key => $voice_array) {
      if ($voice_array['language'] == $language && $voice_array['voice'] == $voice) {
        return $key;
      }
    }
    return NULL;
  }
  
  /**
   * Implementation of VoipServer() API method
   */
  function getDefaultLanguage(){
    $server_config = $this->getConfig();
    $voice_id = VoipVoice::getDefaultVoice()->getVoiceId();
    $voices_available = self::_get_available_voices();
    $voice_descriptor = $voices_available[$voice_id];
    $voice = $voice_descriptor['language'];
    return $voice;
  }

  /**
   * Implementation of VoipServer() API method
   */
  function getDefaultVoice(){
    $server_config = $this->getConfig();
    $voice_id = VoipVoice::getDefaultVoice()->getVoiceId();
    return $voice_id;
  }


  /*
   * API methods
   */

  /**
   * Implementation of the VoipServer dial() API method
   */
  function dial($voipcall, $server_config=NULL) {

    $rc = FALSE;
    voip_error_reset();

    if(!$server_config) {
      $server_config = $this->getConfig();
    }

    // Plivo configuration
    $api_version = 'v0.1';
    $rest_api_url = $server_config['voipplivo_restapi_url'];
    $account_sid = $server_config['voipplivo_account_sid'];
    $auth_token = $server_config['voipplivo_auth_token'];

    // Update call variables
    $voipcall->setSource('drupal');
    $voipcall->setDirection(VoipCall::OUTBOUND);
    
    // save the call in order to get generate an cid
    $voipcall->save();

    // Instantiate a new Plivo Rest Client
    $client = new PlivoRestClient($rest_api_url, $account_sid, $auth_token, $api_version);
    
    # Define Channel Variable - http://wiki.freeswitch.org/wiki/Channel_Variables
    $extra_dial_string = "bridge_early_media=true,hangup_after_bridge=true";

    // Initiate a new outbound call by POST'ing to the Calls resource
    //$plivo_path = "/$api_version/Accounts/$account_sid/Calls";
    $caller_number = ($voipcall->getCallerNumber())
                       ? $voipcall->getCallerNumber()
                       : variable_get('voipcall_cid_number', NULL);
    $dest_number = $voipcall->getDestNumber();

    $menu_path = 'voip/plivo/callhandler';
    $cid = $voipcall->getCid();
    $response_url = $menu_path . '/process_outbound_calls/' . $cid;
    $call_handler_url = url($response_url, array('absolute' => TRUE));
    $response_url = $menu_path . '/process_hangup/' . $cid;
    $status_callback_url = url($response_url, array('absolute' => TRUE));
    
    $gateways = $server_config['voipplivo_call_gateways'];
    $gateway_codecs = $server_config['voipplivo_call_gateway_codec'];
    $gateway_timeouts = $server_config['voipplivo_call_gateway_timeouts'];
    $gateway_retries = $server_config['voipplivo_call_gateway_retries'];

    $vars = array(  
      'From' => $caller_number, # Caller Id
      'To' => $dest_number, # User Number to Call
      'Gateways' => $gateways, # Gateway string to try dialing our separated by comma. First in list will be tried first
      'GatewayCodecs' => $gateway_codecs, # Codec string as needed by FS for each gateway separated by comma
      'GatewayTimeouts' => $gateway_timeouts,      # Seconds to timeout in string for each gateway separated by comma
      'GatewayRetries' => $gateway_retries, # Retry String for Gateways separated by comma, on how many times each gateway should be retried
      'ExtraDialString' => $extra_dial_string,
      'AnswerUrl' => $call_handler_url,
      //TODO: Whats the purpose of hangup and ringing URLs? Is it optional?
      //'HangupUrl' => "http://127.0.0.1:5000/hangup/",
      //'RingUrl' => "http://127.0.0.1:5000/ringing/"
    );

    // Handle answering machine options
   /* if($voipcall->getCheckMachine() == TRUE) {
    // TODO: Is there IfMachine variable in Plivo?
      //$vars['IfMachine'] = 'Continue'; 
    }*/

    $response = $client->call($vars);
    
watchdog('voipplivo','Plivo XML response = '.print_r($response,TRUE));
    // update call based on response
    if($response->IsError){
      // differentiate between processing errors and higher-level errors
      $plivo_error_code = (string)$response->HttpStatus;
      switch($plivo_error_code){
        case '401': // wrong auth token and auth sid or if you your ip is not allowed in ALLOWED_IPS
          $error_msg = t('Wrong auth token and auth sid or ip is not allowed.');
          $voipcall->setErrorMessage($error_msg);
          $voipcall->setCallStatus(VoipCall::ERROR);
          $voipcall->save();
          break;
        default: // processing error
          $plivo_error_msg = $response->ErrorMessage;
          $msg = "Processing failure. Please contact your system's administrator. Plivo error @plivo_code: @plivo_msg";
          $params = array('@plivo_code' => $plivo_error_code, 
                          '@plivo_msg' => $plivo_error_msg);
          $voipcall->report_error('voipplivo', $msg, $params);
          break;
      }
      $rc = FALSE;
    }
    else {
      $rc = TRUE;
      // get the new id and status associated with the call
      //TODO: There is no responseXML
      /*$xml = $response->ResponseXml;
      $call_id = (string)$xml->Call->Sid;
      $plivo_call_status = (string)$xml->Call->Status;
      $voipcall->setCallId($call_id);
      $voipcall = voipplivo_update_call_status($voipcall, $plivo_call_status);*/
      $voipcall->save();

watchdog('voipplivo', 'new call: ' . print_r($voipcall, true));
    }

    return $rc;
  }

  /**
   * Implementation of the VoipServer send_text() API method
   */
  function send_text($text, $voipcall, $reply=FALSE, $server_config=NULL) {
    $rc = FALSE;
    voip_error_reset();
    
    //Determine from/to numbers
    if ($reply) {
      $to = $voipcall->getCallerNumber();
    }
    else {
      $to = $voipcall->getDestNumber();
    }
    
    if (voipscript_use_sms_framework()) {
      //Use SMS Framework to send SMS message if user enabled this option
      voipscript_send_sms_framework($to, $text);
      return TRUE;        
    }
    //Else use VoIP Drupal:
    //Plivo doesn't support SMS yet
    return FALSE;
  }

  /**
   * Implementation of the VoipServer hangup() API method
   */
  function hangup($voipcall, $server_config=NULL) {
    $id = $voipcall->getCallId();
    $status = $voipcall->getCallStatus();
    watchdog('voipplivo', "in hangup() with call id: $id and status: $status");

    $rc = FALSE;
    voip_error_reset();

    if(!$server_config) {
      $server_config = $this->getConfig();
    }

    // Plivo configuration
    $api_version = 'v0.1';
    $rest_api_url = $server_config['voipplivo_restapi_url'];
    $account_sid = $server_config['voipplivo_account_sid'];
    $auth_token = $server_config['voipplivo_auth_token'];

    // Instantiate a new Plivo Rest Client
    $client = new PlivoRestClient($rest_api_url, $account_sid, $auth_token, $api_version);

    $call_id = $voipcall->getCallId();
    if(!$call_id){
      $msg = "Attempt to hangup a Plivo call (cid = @cid) without a call id";
      $params = array('@cid' => $voipcall->getCid());
      $voipcall->report_error('voipplivo', $msg, $params);
      $rc = FALSE;
    }
    else {
      //$plivo_path = "/$api_version/Accounts/$account_sid/Calls/$call_id";
      $vars =    array(
        'CallUUID' => $call_id, # Request UUID to hangup call
      );

      $response = $client->hangup_call($vars);

      // update call based on response
      if($response->IsError){
        $msg = "Plivo hangup command failed: @error";
        $params = array('@error' => $response->ErrorMessage);
        $voipcall->report_error('voipplivo', $msg, $params);
      }
      else {
        $voipcall->setHangupReason(VoipCall::HANGUP_API_REQUEST);
        $voipcall->save();
        $rc = TRUE;
      }
    }

    return $rc;
  }

   /**
   * Implementation of the VoipServer text_handler() API method
   */
  function text_handler($text_content, $origin ,$destination, $time, $network, $server_name) {
    $menu_path = 'voip/plivo/callhandler';
    $request_url = $menu_path . '/process_inbound_text/';
    $url = url($request_url, array('absolute' => TRUE));
    
    $server_config = $this->getConfig();
    $vars = array(
      'AccountSid' =>  $server_config['voipplivo_account_sid'],
      'SmsSid' => $todo,
      'From' => $origin,
      'To' => $destination,
      'Body' => $text_content,
      'Direction' => 'inbound',
      'Channel' => 'offline_text'
    );
    
    //url-ify the data for the POST
   foreach ($vars as $key => $value) { 
     $fields_string .= $key.'='.urlencode($value).'&'; 
   }
   $fields_string = rtrim($fields_string,'&');
   
   //open connection
   $ch = curl_init();

   //set the url, number of POST vars, POST data
   curl_setopt($ch, CURLOPT_URL, $url);
   curl_setopt($ch, CURLOPT_POST,count($vars));
   curl_setopt($ch, CURLOPT_POSTFIELDS, $fields_string);
   curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

   //execute post
   $result = curl_exec($ch);

   //close connection
   curl_close($ch);
  }

  /**
   * Implementation of the VoipServer ping() API method
   */
  function ping($server_config=NULL) {

    if(!$server_config) {
      $server_config = $this->getConfig();
    }

    // Plivo configuration
    $api_version = 'v0.1';
    $rest_api_url = $server_config['voipplivo_restapi_url'];
    $account_sid = $server_config['voipplivo_account_sid'];
    $auth_token = $server_config['voipplivo_auth_token'];

    // Instantiate a new Plivo Rest Client
    $client = new PlivoRestClient($rest_api_url, $account_sid, $auth_token, $api_version);

    // Request a reload configuration as a way to test the server connectivity
    voip_error_reset();
    $vars = array();
    $response = $client->reload_config($vars);

    if($response->IsError){
      voip_error("Plivo command Ping failed: " . $response->ErrorMessage);
      return FALSE;
    }
    return TRUE;
  }

  /**
   * Private methods
   */
// TODO:  private static function _get_available_voices() {
static function _get_available_voices() {
    // Note: check http://festvox.org/voicedemos.html for demos of the different voices
    $voices = array();
    $voices['slt'] = array('language' => 'en', 'voice' => 'woman',
                        'description' => t('Slt (female voice in American English)'));
    $voices['rms'] = array('language' => 'en', 'voice' => 'man',
                        'description' => t('Rms (male voice in American English)'));
    $voices['awb'] = array('language' => 'en2', 'voice' => 'man',
                        'description' => t('Awb (male voice in Scottish English)'));
    $voices['kal'] = array('language' => 'en-us', 'voice' => 'man',
                        'description' => t('Kal (male voice in American English)'));
    return $voices;
  }
}

