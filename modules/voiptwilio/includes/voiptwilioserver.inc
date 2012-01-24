<?php

// $Id$

/**
 * @file
 * Implementation of the VoipTwilioServer class
 */


/**
 * File dependencies
 */

foreach (array('twilio.php', 'voiptwilio.inc') as $file) {
  require_once(dirname(__FILE__) . DIRECTORY_SEPARATOR . $file);
}

module_load_include('inc', 'voip', 'includes/voipserver');
module_load_include('inc', 'voipcall', 'voipcall');


/**
 * Class definitions
 */
        
class VoipTwilioServer extends VoipServer {

  /*
   * Constructors
   */

  function __construct() {
    $id = 'twilio';
    $name = 'Twilio';
    $config_form = 'voiptwilio_admin_form';
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
   * Implementation of VoipServer() API method
   */
  function getDefaultLanguage(){
    $voice = VoipVoice::getDefaultVoice();
    return $voice->getLanguage();
   /* $voices_available = self::_get_available_voices();
    $voice_descriptor = $voices_available[$voice_id];
    $voice = $voice_descriptor['language'];
    return $voice;*/
  }

  /**
   * Implementation of VoipServer() API method
   */
  function getDefaultVoice(){
    $voice = VoipVoice::getDefaultVoice();
    return $voice->getGender();
   /* $voices_available = self::_get_available_voices();
    $voice_descriptor = $voices_available[$voice_id];
    $voice = $voice_descriptor['voice'];
    return $voice;*/
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

    // Twilio configuration
    $api_version = '2010-04-01';
    $account_sid = $server_config['voiptwilio_account_sid'];
    $auth_token = $server_config['voiptwilio_auth_token'];

    // Update call variables
    $voipcall->setSource('drupal');
    $voipcall->setDirection(VoipCall::OUTBOUND);
    
    // save the call in order to get generate an cid
    $voipcall->save();

    // Instantiate a new Twilio Rest Client
    $client = new TwilioRestClient($account_sid, $auth_token);

    // Initiate a new outbound call by POST'ing to the Calls resource
    $twilio_path = "/$api_version/Accounts/$account_sid/Calls";
    $caller_number = ($voipcall->getCallerNumber())
                       ? $voipcall->getCallerNumber()
                       : variable_get('voipcall_cid_number', NULL);
    $dest_number = $voipcall->getDestNumber();

    $menu_path = 'voip/twilio/callhandler';
    $cid = $voipcall->getCid();
    $response_url = $menu_path . '/process_outbound_calls/' . $cid;
    $call_handler_url = url($response_url, array('absolute' => TRUE));
    $response_url = $menu_path . '/process_hangup/' . $cid;
    $status_callback_url = url($response_url, array('absolute' => TRUE));

    $vars = array(
      'Caller' => $caller_number,
      'Called' => $dest_number,
      'Url' => $call_handler_url,
      'Method' => 'POST',
      'StatusCallback' => $status_callback_url,
      'StatusCallbackMethod' => 'POST',
      'SendDigits' => '',  // TODO: shall we encode extra digits with dest_number?
      'Timeout' => 60 // TODO: Set to 15 in order to avoid answering machines?
    );

    // Handle answering machine options
    if($voipcall->getCheckMachine() == TRUE) {
// TODO: do we need to implement Twilio's 'hangup on machine' option here?
      // Note: adding any value in here -- even 'Continue' -- adds a 5 sec delay
      //       to the call. To prevent that, 'IfMachine' has to be NULL
      $vars['IfMachine'] = 'Continue'; 
    }

    $response = $client->request($twilio_path, "POST", $vars);

    // update call based on response
    if($response->IsError){
      // differentiate between processing errors and higher-level errors
      $twilio_error_code = (string)$response->ResponseXml->RestException->Code;
      switch($twilio_error_code){
        // the following codes are listed in
        //   http://www.twilio.com/docs/errors/reference/
        case '13214': // Dial: Invalid callerId value
        case '21210': // Caller phone number not verified
        case '21212': // Invalid Caller Phone Number
          $error_msg = t('Invalid caller id value');
          $voipcall->setErrorMessage($error_msg);
          $voipcall->setCallStatus(VoipCall::ERROR);
          $voipcall->save();
          break;
        case '13223': // Dial: Invalid phone number format
        case '13224': // Dial: Invalid phone number
        case '21211': // Invalid Called Phone Number
        case '21213': // Caller phone number is required
        case '21401': // Invalid Phone Number
          $error_msg = t('Invalid phone number');
          $voipcall->setErrorMessage($error_msg);
          $voipcall->setCallStatus(VoipCall::ERROR);
          $voipcall->save();
          break;
        case '13225': // Dial: Forbidden phone number 
        case '21215': // Account not authorized to call NNN
          $error_msg = t('Forbidden phone number');
          $voipcall->setErrorMessage($error_msg);
          $voipcall->setCallStatus(VoipCall::ERROR);
          $voipcall->save();
          break;
        default: // processing error
          $twilio_error_msg = $response->ErrorMessage;
          $msg = "Processing failure. Please contact your system's administrator. Twilio error @twilio_code: @twilio_msg";
          $params = array('@twilio_code' => $twilio_error_code, 
                          '@twilio_msg' => $twilio_error_msg);
          $voipcall->report_error('voiptwilio', $msg, $params);
          break;
      }
      $rc = FALSE;
    }
    else {
      $rc = TRUE;

      // get the new id and status associated with the call
      $xml = $response->ResponseXml;
      $call_id = (string)$xml->Call->Sid;
      $twilio_call_status = (string)$xml->Call->Status;
      $voipcall->setCallId($call_id);
      $voipcall = voiptwilio_update_call_status($voipcall, $twilio_call_status);
      $voipcall->save();

watchdog('voiptwilio', 'new call: ' . print_r($voipcall, true));
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
      $from = ($voipcall->getDestNumber())
                       ? $voipcall->getDestNumber()
                       : variable_get('voipcall_cid_number', NULL);
    }
    else {
      $to = $voipcall->getDestNumber();
      $from = ($voipcall->getCallerNumber())
                       ? $voipcall->getCallerNumber()
                       : variable_get('voipcall_cid_number', NULL);
    }
    
    if (voipscript_use_sms_framework()) {
      //Use SMS Framework to send SMS message if user enabled this option
      voipscript_send_sms_framework($to, $text);
      return TRUE;        
    }
    //Else use VoIP Drupal:
    if (!$server_config) {
      $server_config = $this->getConfig();
    }

    // Twilio configuration
    $api_version = '2010-04-01';
    $account_sid = $server_config['voiptwilio_account_sid'];
    $auth_token = $server_config['voiptwilio_auth_token'];

    // Update call variables
    $voipcall->setSource('drupal');
    $voipcall->setDirection(VoipCall::OUTBOUND);
    
    // save the call
    $voipcall->save();

    // Instantiate a new Twilio Rest Client
    $client = new TwilioRestClient($account_sid, $auth_token);

    // Initiate a new outbound text by POST'ing to the SMS/Messages resource
    $twilio_path = "/$api_version/Accounts/$account_sid/SMS/Messages";
    $menu_path = 'voip/twilio/callhandler';
    $cid = $voipcall->getCid();
    $response_url = $menu_path . '/process_sms_callback/' . $cid;
    $status_callback_url = url($response_url, array('absolute' => TRUE));
    
    $vars = array(
      'From' => $from,
      'To' => $to,
      'Body' => $text,
      'Method' => 'POST',
      'StatusCallback' => $status_callback_url,
    );

    $response = $client->request($twilio_path, "POST", $vars);

    // update call based on response
    if($response->IsError){
      // differentiate between processing errors and higher-level errors
      $twilio_error_code = (string)$response->ResponseXml->RestException->Code;
      switch($twilio_error_code){
        // the following codes are listed in
        //   http://www.twilio.com/docs/errors/reference/
        case '13214': // Dial: Invalid callerId value
        case '21210': // Caller phone number not verified
        case '21212': // Invalid Caller Phone Number
          $error_msg = t('Invalid caller id value');
          $voipcall->setErrorMessage($error_msg);
          $voipcall->setCallStatus(VoipCall::ERROR);
          $voipcall->save();
          break;
        case '13223': // Dial: Invalid phone number format
        case '13224': // Dial: Invalid phone number
        case '21211': // Invalid Called Phone Number
        case '21213': // Caller phone number is required
        case '21401': // Invalid Phone Number
          $error_msg = t('Invalid phone number');
          $voipcall->setErrorMessage($error_msg);
          $voipcall->setCallStatus(VoipCall::ERROR);
          $voipcall->save();
          break;
        case '13225': // Dial: Forbidden phone number 
        case '21215': // Account not authorized to call NNN
          $error_msg = t('Forbidden phone number');
          $voipcall->setErrorMessage($error_msg);
          $voipcall->setCallStatus(VoipCall::ERROR);
          $voipcall->save();
          break;
        default: // processing error
          $twilio_error_msg = $response->ErrorMessage;
          $msg = "Processing failure. Please contact your system's administrator. Twilio error @twilio_code: @twilio_msg";
          $params = array('@twilio_code' => $twilio_error_code, 
                          '@twilio_msg' => $twilio_error_msg);
          $voipcall->report_error('voiptwilio', $msg, $params);
          break;
      }
      $rc = FALSE;
    }
    else {
      $rc = TRUE;

      // get the new id and status associated with the call
      $xml = $response->ResponseXml;
      $call_id = (string)$xml->SMSMessage->Sid;
      $twilio_call_status = (string)$xml->SMSMessage->Status;
      $voipcall->setCallId($call_id);
      $voipcall = voiptwilio_update_call_status($voipcall, $twilio_call_status);
      $voipcall->save();

      watchdog('voiptwilio', 'new text: ' . print_r($voipcall, true));
    }

    return $rc;
  }

  /**
   * Implementation of the VoipServer hangup() API method
   */
  function hangup($voipcall, $server_config=NULL) {
$id = $voipcall->getCallId();
$status = $voipcall->getCallStatus();
watchdog('voiptwilio', "in hangup() with call id: $id and status: $status");

    $rc = FALSE;
    voip_error_reset();

    if(!$server_config) {
      $server_config = $this->getConfig();
    }

    // Twilio configuration
    $api_version = '2010-04-01';
    $account_sid = $server_config['voiptwilio_account_sid'];
    $auth_token = $server_config['voiptwilio_auth_token'];

    // Instantiate a new Twilio Rest Client
    $client = new TwilioRestClient($account_sid, $auth_token);

    $call_id = $voipcall->getCallId();
    if(!$call_id){
      $msg = "Attempt to hangup a Twilio call (cid = @cid) without a call id";
      $params = array('@cid' => $voipcall->getCid());
      $voipcall->report_error('voiptwilio', $msg, $params);
      $rc = FALSE;
    }
    else {
      $twilio_path = "/$api_version/Accounts/$account_sid/Calls/$call_id";
      $status = 'completed';
      $vars =    array(
        'Status' => $status,
      );

      $response = $client->request($twilio_path, "POST", $vars);

      // update call based on response
      if($response->IsError){
        $msg = "Twilio hangup command failed: @error";
        $params = array('@error' => $response->ErrorMessage);
        $voipcall->report_error('voiptwilio', $msg, $params);
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
    $menu_path = 'voip/twilio/callhandler';
    $request_url = $menu_path . '/process_inbound_text/';
    $url = url($request_url, array('absolute' => TRUE));
    
    $server_config = $this->getConfig();
    $vars = array(
      'AccountSid' =>  $server_config['voiptwilio_account_sid'],
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

    // Twilio configuration
    $api_version = '2010-04-01';
    $account_sid = $server_config['voiptwilio_account_sid'];
    $auth_token = $server_config['voiptwilio_auth_token'];

    // Instantiate a new Twilio Rest Client
    $client = new TwilioRestClient($account_sid, $auth_token);

    // Request a call log as a way to test the server connectivity
    voip_error_reset();
    $path = "/$api_version/Accounts/$account_sid/Calls";
    $vars = array();
    $response = $client->request($path, "GET", $vars);

    if($response->IsError){
      voip_error("Twilio command Ping failed: " . $response->ErrorMessage);
      return FALSE;
    }
    return TRUE;
  }

 static function _get_available_voices() {
    $voices = array();
    $voices['dave'] = array('language' => 'en', 'voice' => 'man',
                        'description' => t('Dave (male voice in English)'));
    $voices['allison'] = array('language' => 'en', 'voice' => 'woman',
                        'description' => t('Allison (female voice in English)'));
    $voices['jorge'] = array('language' => 'es', 'voice' => 'man',
                        'description' => t('Jorge (male voice in Spanish)'));
    $voices['carmen'] = array('language' => 'es', 'voice' => 'woman',
                        'description' => t('Carmen (female voice in Spanish)'));
    $voices['bernard'] = array('language' => 'fr', 'voice' => 'man',
                        'description' => t('Bernard (male voice in French)'));
    $voices['florence'] = array('language' => 'fr', 'voice' => 'woman',
                        'description' => t('Florence (female voice in French)'));
    $voices['stefan'] = array('language' => 'de', 'voice' => 'man',
                        'description' => t('Stefan (male voice in German)'));
    $voices['katrin'] = array('language' => 'de', 'voice' => 'woman',
                        'description' => t('Katrin (female voice in German)'));
    return $voices;
  }
}