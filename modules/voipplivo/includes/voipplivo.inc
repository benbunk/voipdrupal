<?php
// $id$

/**
 * @file
 * Implementation of Plivo-related functionality
 */

foreach (array('plivo.php') as $file) {
  require_once(dirname(__FILE__) . DIRECTORY_SEPARATOR . $file); 
}


/**
 * Internal functions
 */

/*
 * Process incoming requests and responses from Plivo
 *
 * @param string $plivo_action set to the action to be executed. 
 *   It can assume any of the following values:
 *   - process_conference, to handle responses to conference call requests
 *   - process_inbound_calls, to handle incoming calls
 *   - process_inbound_text, to handle incoming text
 *   - process_outbound_calls, to handle the response to outgoing call requests
 *   - process_dial, to handle the response to the "dial" script command
 *   - process_get_input, to handle responses to "get input" script commands
 *   - process_hangup, to handle call termination
 *   - process_record, process_record_mp3 and process_empty_recording, to handle
 *     responses to 'record' script commands
 *   - process_wait, to continue after wait command
 *   - process_send_sms, to handle responses to 'send sms' script commands
 *   - process_sms_callback, to handle final responses to 'send sms'.
 *
 * @param string $cid with the cid of the call
 *
 * @return void
 */
function _voipplivo_call_handler($plivo_action, $cid=NULL) {

$msg = "in voipplivo_call_handler() with plivo_action: $plivo_action, cid: $cid, and _REQUEST: " . print_r($_REQUEST, TRUE);
$vars = array();
watchdog('voipplivo', $msg, $vars, WATCHDOG_INFO);

  // validate $_REQUEST to avoid hack attempts
  if (!_voipplivo_validate_request()) {
    $msg = '_voip_plivo_call_handler called with invalid AccountSid: @sid';
    $params = array('@sid' => $incoming_sid);
    watchdog('voipplivo', $msg, $params, WATCHDOG_WARNING);
    return FALSE;  // TODO: should we return an error response to the server?
  }
  
  //Removing text options:
  /*if ($plivo_action == 'process_inbound_text') {
    if (isset($_REQUEST['Channel'])) {
      $channel = $_REQUEST['Channel'];
    }
    else {
      $channel = 'text';
    }
    
    $network = 'sms';
    $from = $_REQUEST['From'];
    $to = $_REQUEST['To'];
  }
  else {*/
    $channel = 'voice';
    $network = 'voip';
    $from = $_REQUEST['From'];
    $to = $_REQUEST['To'];
  //}

  //At hangup the response returns only From value, making it impossible to track session, so making this hack to prevent creating new sessions.
 /*if($_REQUEST['HangupCause'] == 'NORMAL_CLEARING') {
    return;
  }*/
  if (!$cid) {
    $voip_session = VoipSession::getSessionByNumber($from, $to);
//watchdog('voipplivo', "Session without cid, from: $from, to: $to, url:$plivo_action and SESSION: " . print_r($voip_session, TRUE));
    if ($voip_session) {
      $cid = $voip_session->getCid();
      // load the variables already associated with the call, if any
      $call = VoipCall::load($cid);
      //watchdog('voipplivo', "Session Loaded cid: $cid call url:$plivo_action " . print_r($call, TRUE));

      $voip_session->resetTimer();
      $voip_session->save();
    }
    else {
      $call = new VoipCall();
      $voip_session = new VoipSession();
      $voip_session->setOriginNumber($from);
      $voip_session->setDestinationNumber($to);
      $voip_session->save();
      }
  }
  else {
    $voip_session = VoipSession::getSessionByCid($cid);
    //watchdog('voipplivo', "Session with cid $cid from: $from, to: $to, url:$plivo_action and SESSION: " . print_r($voip_session, TRUE));

    if ($voip_session) {
      $voip_session->resetTimer();
      $voip_session->save();
    }
    else if ($from && $to && $plivo_action!='process_hangup'){
      $voip_session = VoipSession::getSessionByNumber($from, $to);
      if ($voip_session) {
        $cid = $voip_session->getCid();
        // load the variables already associated with the call, if any
        $call = VoipCall::load($cid);
        $voip_session->resetTimer();
        $voip_session->save();
      }
      else {
        $voip_session = new VoipSession();
        $voip_session->setOriginNumber($from);
        $voip_session->setDestinationNumber($to);
        $voip_session->save();
      }
    }
  }
  
  // handle incoming call and text requests
  if($plivo_action == 'process_inbound_calls') {
    //$call = new VoipCall();
    $call_sid = $_REQUEST['CallUUID']; // unique identifier of the call, 34 char CallUUID
    $call->setCallId($call_sid);
    $call->setSource('plivo');
    $dir_tmp = ($_REQUEST['Direction'] == 'inbound')
                         ? VoipCall::INBOUND
                         : VoipCall::OUTBOUND_DIAL;
    $call->setDirection($dir_tmp);
    $call->setAnsweredBy(VoipCall::ANSWERED_DRUPAL);
    $call->setCallStatus(VoipCall::RINGING);
    $call->setStartTime(time());
    $call->setCurrentChannel($channel);
    $call->setCurrentNetwork($network);
    // save the call
    $call->save();
    // get the cid of the call
    $cid = $call->getCid();
  }
  /*else if ($plivo_action == 'process_inbound_text') {
    $call_sid = $_REQUEST['SmsSid']; // unique identifier of the text, 34 char
    $call->setCallId($call_sid);
    $call->setSource('plivo');
    $dir_tmp = VoipCall::INBOUND;
    $call->setDirection($dir_tmp);
    $call->setAnsweredBy(VoipCall::ANSWERED_DRUPAL);
    $call->setCallStatus(VoipCall::IN_PROGRESS);
    $call->setStartTime(time());
    
    $call->setCallerNumber($from);
    $call->setDestNumber($to);
    $call->setCurrentChannel($channel);
    $call->setCurrentNetwork($network);
    $call->save();
  }*/
  else {
    // handle ongoing calls
    // 'process_hangup' for incoming calls does not come with a $cid
    if($plivo_action == 'process_hangup') {
      if(!$cid) {
        $call_id = $_REQUEST['CallUUID'];
        $cid = VoipCall::getCidFromCallId($call_id);
      }
    }

    // load the variables already associated with the call, if any
    $call = VoipCall::load($cid);

    // handle first response to an outbound call
    if($plivo_action == 'process_outbound_calls') {
      // store plivo call id for all outbound calls
      $call_id = $_REQUEST['CallUUID'];
      $call->setCallId($call_id);
      //TODO: Is there AnsweredBy in Plivo?
     /* $answered_by = $_REQUEST['AnsweredBy'];
      if($answered_by) {
        $tmp = ($answered_by == 'human') ? VoipCall::ANSWERED_HUMAN
                                         : VoipCall::ANSWERED_MACHINE; 
        $call->setAnsweredBy($tmp);
      }*/
    }
  }
  
  //Left because offline_text
  if (in_array($channel, array('offline_text','text'))) {
    //Set text-related variables in the call object 
    $call->setTextVariables($_REQUEST['Body'], $_REQUEST['From'], $_REQUEST['To'], $network);
  }
  
  // get the cid of the call
  $cid = $call->getCid();
  if($voip_session && !$voip_session->getCid()){
    $voip_session->setCid($cid);
    $voip_session->save();
  }
  
  // update common call variables
  if(!$call->getCurrentChannel())
     $call->setCurrentChannel($channel);
  if(!$call->getCurrentNetwork())
     $call->setCurrentNetwork($network);
  if(!$call->getCallerNumber())
    $call->setCallerNumber($_REQUEST['From']);
  if(!$call->getCallerName())
    $call->setCallerName($_REQUEST['CallerName']);
  if(!$call->getDestNumber())
    $call->setDestNumber($_REQUEST['To']);
  
  $plivo_call_status = $_REQUEST['CallStatus']; 
 /* if (empty($plivo_call_status)) {
    $plivo_call_status = $_REQUEST['SmsStatus']; 
  }*/
 
  // set start time for the call
  if($plivo_call_status == 'in-progress'){
    $start_time = $call->getStartTime();
    if(!$start_time) {
      // start counting the call duration
      $call->setStartTime();
    }
  }

//watchdog('voipplivo', "plivo status: $plivo_call_status, call status: " . $call->getCallStatus());

  // update call status
  // NOTE: sms callbacks do not have a call status associated with them
  /*if ($plivo_action == 'process_sms_callback' || $plivo_action == 'process_inbound_text' || $plivo_action == 'process_send_sms'){
    $call->SetCallStatus(VoipCall::IN_PROGRESS);
  }
  else{*/
    //TODO: Check if Plivo statuses are same as Twilio?
    watchdog('voipplivo', 'Check if Plivo statuses are same as Twilio '.print_r($plivo_call_status, true));
    $call = voipplivo_update_call_status($call, $plivo_call_status);
 // }

  // update script variables with call location, if available
 /* if(isset($_REQUEST['FromCity'])){
    $call_location = array();
    $call_location['caller_city'] = $_REQUEST['FromCity'];
    $call_location['caller_state'] = $_REQUEST['FromState'];
    $call_location['caller_zip'] = $_REQUEST['FromZip'];
    $call_location['caller_country'] = $_REQUEST['FromCountry'];
    $call_location['called_city'] = $_REQUEST['ToCity'];
    $call_location['called_state'] = $_REQUEST['ToState'];
    $call_location['called_zip'] = $_REQUEST['ToZip'];
    $call_location['called_country'] = $_REQUEST['ToCountry'];
    $script = $call->getScript();
    $script->setVar('call_location', $call_location);
    $call->setScript($script);
  }*/

  $is_hangup = $call->isHangup();

//watchdog('voipplivo', "plivo status: $plivo_call_status, updated call status: " . $call->getCallStatus() . ", is_hangup: $is_hangup");

  // process responses to requests previously-issued by the call script
  if(!$is_hangup) {
    switch($plivo_action) {
      default:
        $msg = 'Invalid plivo_action: @action';
        $params = array('action' => $plivo_action);
        $call->report_error('voipplivo', $msg, $params);
        return FALSE;
        break;
      case 'process_inbound_calls':
      case 'process_outbound_calls':
      case 'process_hangup':
      //case 'process_inbound_text':
      case 'process_wait':
        // nothing special to be done here. the above commands have already
        // been processed in the previous lines
        break;
      case 'process_conference':
        // NOTE: in Plivo, conference calls are executed via the dial command
        //       and, therefore, should be processed in the same way
      case 'process_dial':
      //TODO: Test request parameters
      watchdog('voipplivo', "Dial response: " .print_r($_REQUEST, true));
        $call = _voipplivo_process_dial_response($call, $_REQUEST);
        break;
      case 'process_get_input':
        $script = $call->getScript();
        $input = ($_REQUEST['Digits'] == 'no_input')
                 ? '' :  $_REQUEST['Digits'];
        $script->setVar('input_digits', $input);
        $call->setScript($script);
        break;
      case 'process_record':
      case 'process_record_mp3':
        $script = $call->getScript();
        $plivo_record_url = $_REQUEST['RecordFile'];
        //TODO: is only local path supported?
        watchdog('voipplivo', "Record response: " .print_r($_REQUEST, true));
        //Transfer the file to local server
        $file = _voipplivo_transfer_file($plivo_record_url);     
        $script->setVar('recording_path', file_create_url($file['filepath']));
        //TODO: For now we set this to same recording_path because public url is not available in Plivo
        $script->setVar('recording_public_url', file_create_url($file['filepath']));  
        $script->setVar('recording_duration', $_REQUEST['RecordingDuration']);
        $script->setVar('recording_digits', $_REQUEST['Digits']);
        $script->setVar('recording_fid', $file['fid']);
        $call->setScript($script);
        break;
      case 'process_empty_recording':
        $script = $call->getScript();
        $script->setVar('recording_path', '');
        $script->setVar('recording_public_url', '');
        $script->setVar('recording_duration', 0);
        $script->setVar('recording_digits', NULL);
        $script->setVar('recording_fid', NULL);
        $call->setScript($script);
        break;

     /* case 'process_send_sms':
      case 'process_sms_callback':
        $call = _voipplivo_process_sms_response($call, $_REQUEST);
        if(strtolower($call->getCurrentChannel()) == 'text') {
          // update sms status
          $call->save();
          // since 'process_sms_callback' is asynchronous, go back
          if($plivo_action == 'process_sms_callback') {
            return;
          }
        }
        break;*/
    }
  }

  if($is_hangup) {
  
    // determine call duration
    $end_time = $call->setEndTime();
    $start_time = $call->getStartTime();
    
    if($start_time) {
      $duration  = $end_time - $start_time;
    }
    else {
      //Not default param, but you can set in plivo config :EXTRA_FS_VARS = variable_duration
     // get call duration from Plivo
     $duration = isset($_REQUEST['variable_duration'])?$_REQUEST['variable_duration']:NULL;
    }
    $call->setDuration($duration);
  }

  // update the voipcall with the new values
  $call = $call->save();

  /*if ($plivo_action == 'process_inbound_text') {
    $script = $call->getScript();
    $text_handler = $script->getVar('text_handler');
    if ($text_handler) {
      $message = $script->getVar('inbound_text_contents');
      $res = call_user_func_array($text_handler, array($message, $call->getCallerNumber(), $call->getDestNumber(), $call, $call->getCurrentNetwork()));
      return;
    }
    else if($call->getCallChannel() != 'text') {
      return;
    }
  }*/
  // process the call script
  $rc = _voipplivo_script_handler($call);

  return;  // NOTE: this function MUST NOT return any value.  If it does, the
           //       output to Plivo will get messed up with Drupal hmtl...
}

/**
 * Process voipcall scripts
 *
 * @param $call, VoipCall instance with the call information
 *
 * @return boolean.  TRUE on success or FALSE on failure.  Check voip_error() for processing failure.
 */
function _voipplivo_script_handler($call) {
$msg = 'Entering script_handler with voipcall: ' . print_r($call,true);
watchdog('voipplivo', $msg, array(), WATCHDOG_INFO);

  $rc = TRUE;

  // get the script object associated with the call
  $script = $call->getScript();

  // check if call is in a hangup state
  $hangup = $call->isHangup();
  if($hangup) {
    $hangup_script_name = $call->getHangupScriptName();
    // Stop processing if no hangup script defined
    if(!$hangup_script_name) {
      return TRUE;
    }
    else {
      // load hangup script commands
      $rc = $script->runGosub($hangup_script_name);
      if($script->isError()){
        $msg = $script->getErrorMessage();
        $params = array();
        $call->report_error('voipplivo', $msg, $params);
        return FALSE;
      }
    }
  }

 // process script commands
  $processing = TRUE;
  $response = '';
  while($processing) {
    $cmd = $script->getNextCommand();
    if(!$cmd) {
      $processing = FALSE;

      // no more commands to be processed    
      $stack_level = $script->getStackLevel();
      if ($stack_level>0) {
        //return to previous stack
        $script->runReturn();
        $processing = TRUE;
      }
      else if ($call->getCurrentChannel() != 'text') {
        // for voice channel send a final hangup request 
        $response .= '<Hangup/>';
        $msg = 'Script @name ended without a "hangup" command.';
        $params = array('@name' => $script->getName());
        watchdog('voipplivo', $msg, $params, WATCHDOG_WARNING);
        $call->setHangupReason(VoipCall::HANGUP_SCRIPT_END);
      }
      //for text channel don't do anything
    }

    // process current command
    else {
      $cmd_id = $cmd->getId();
   
      switch($cmd_id) {
        default:
          $msg = "Invalid command id '@cmd_id' in script '@script_name': @script";
          $params = array('@cmd_id' => $cmd_id, '@script_name' => $script->getName(), '@script' => print_r($script,true));
          $call->report_error('voipplivo', $msg, $params);
          $rc = FALSE;
          $processing = FALSE;
          break;

        case 'VoipCmdBeep';
           $beep_url = url('voip/sound/beep', array('absolute' => TRUE));
           $response .= "<Play>$beep_url</Play>";
        break;
        
        case 'VoipCmdDial';
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $number = $script->evalString($cmd->getParam('number'));
          if ($call->getCurrentChannel() == 'text' || $call->getCurrentChannel() == 'offline_text') {
            //In case VoipCmdDial is called within text channel then instead of transfer we call the sender.
            //we are starting a call, hangup() the existing session
            $response .= '<Hangup/>';
            
            if (!$number) {
              $number = $call->getCallerNumber();
            }
            $caller_number = variable_get('voipcall_cid_number', NULL);
            $call->setDestNumber($number);
            $call->setCallerNumber($caller_number);
            $call->setScript($script);
            $call->save();
            voip_dial($call);
            $processing = FALSE;
          }
          else {
            $menu_path = 'voip/plivo/callhandler';
            $cid = $call->getCid();
            $response_url = $menu_path . '/process_dial/' . $cid ;
            $action = url($response_url, array('absolute' => TRUE));
            $method = 'POST';
          
            // TODO: implement the other features such as multi-dial, url(for private scripts) and sendDigits?
            $timeout = $script->evalString($cmd->getParam('timeout', 30));
            $hangup_on_star =  $script->evalString($cmd->getParam('hangup_on_star', false));
            $hangup_on_star =  $hangup_on_star? 'true':'false';
            $time_limit = $script->evalString($cmd->getParam('time_limit', '14400'));
            $caller_number = $script->evalString($script->getVar('caller_number'));
            $caller_id = $script->evalString($cmd->getParam('caller_id', $caller_number)); 
            $response .= '<Dial action="' . $action
                            . '" method="' . $method
                            //TODO: No timeout in Plivo dial.
                            //. '" timeout="' . $timeout
                            . '" hangupOnStar="' . $hangup_on_star
                            . '" timeLimit="' . $time_limit
                            . '" callerId="' . $caller_id
                            . '" >';
            $response .= '<Number>' . $number . '</Number>';
            $response .= '</Dial>';
          }
          
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdGetInput':
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/plivo/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_get_input/' . $cid;
          $action = url($response_url, array('absolute' => TRUE));
          $method = 'POST';
          $timeout = $script->evalString($cmd->getParam('timeout', 5)); 
          $end_key = $script->evalString($cmd->getParam('end_key', '')); 
          $num_digits = $script->evalString($cmd->getParam('num_digits')); 
          $prompt = $cmd->getParam('prompt'); 
          $response .= '<GetDigits action="' . $action
                            . '" method="' . $method
                            . '" timeout="' . $timeout
                            . '" finishOnKey="' . $end_key;
          if($num_digits) {
            $response .=      '" numDigits="' . $num_digits;
          }
          $response .= '" >';
          if($prompt) {
            $response .=  _voipplivo_generate_audio_response($script, $prompt);
          }
          $response .= '</GetDigits>';
          // in case of timeout or empty input, set Digits to 'no_input'
          $action_timeout = $action . '&amp;Digits=' . 'no_input';
          $response .= '<Redirect method="GET">'
                         . $action_timeout
                         . '</Redirect>';
          $rc = TRUE;
          $processing = FALSE;
          break;
        case 'VoipCmdGetVoiceInput':
          //TODO: http://www.plivo.org/docs/restxml/voice-elements/getspeech/
          $rc = TRUE;
          $processing = FALSE;
        break;  

        case 'VoipCmdGosub':
          $script_name = $script->evalString($cmd->getParam('script_name'));
          $options = $script->evalString($cmd->getParam('options'));
          $rc = $script->runGosub($script_name, $options);
          if($script->isError()){
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voipplivo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            $text = 'Processing error. Please contact your system administrator.';
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $response .= "<Speak>$text</Speak>";
          }
          break;

        case 'VoipCmdGoto';
          $label_name = $script->evalString($cmd->getParam('label_name', ''));
          $rc = $script->runGoto($label_name);
          if($script->isError()){
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voipplivo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            $text = 'Processing error. Please contact your system administrator.';
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $response .= "<Speak>$text</Speak>";
          }
          break;

        case 'VoipCmdGotoIf';
          $condition = $script->evalString($cmd->getParam('condition', ''));
          if($condition) { 
            $label_name = $script->evalString($cmd->getParam('label_name', ''));
            $rc = $script->runGoto($label_name, $condition);
            if($script->isError()){
              $msg = $script->getErrorMessage();
              $params = array();
              $call->report_error('voipplivo', $msg, $params);
              $rc = FALSE;
              $processing = FALSE;
              $found = TRUE;
              $text = 'Processing error. Please contact your system administrator.';
              $langcode = $script->getLanguage();
              $text = t($text, NULL, $langcode);
              $response .= "<Speak>$text</Speak>";
            }
          }
          break;

        case 'VoipCmdHangup':
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $call->setHangupReason(VoipCall::HANGUP_SCRIPT_COMMAND);
          
          $hangup_mode = $script->evalString($cmd->getParam('hangup_mode'));
          
          switch ($hangup_mode) {
            case 'resume':
             if ($call->getCurrentChannel() != 'text') {
               // send the hangup request and stop processing script commands
               // Note: this will trigger a response from Plivo with $plivo_action = process_hangup and CallStatus = completed
               $response .= '<Hangup />';
             }
            break;
            case 'reset':
              //Reset script
              $script->resetIndex();
            break;
            case 'end_session':
              //Kill the session if any
              $cid = $call->getCid();
              $session = VoipSession::getSessionByCid($cid);
              if ($session) {
                $session->destroy();
              }
              
              if ($call->getCurrentChannel() != 'text') {
                // send the hangup request and stop processing script commands
                // Note: this will trigger a response from Plivo with $plivo_action = process_hangup and CallStatus = completed
                $response .= '<Hangup/>';
              }
            break;
          }

         
           
          $processing = FALSE;
          break;

        case 'VoipCmdJoinConference';
          // NOTE: Plivo offers several conference call parameters that are not
          //       supported by the VoIP Drupal API. In order to make those
          //       options available, the developer would have to extend the
          //       VoipScript class into something like VoipScriptPlivo.
          //       However, the specific commands of such class would not be 
          //       supported by other VoIP servers.
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/plivo/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_conference/' . $cid;
          $action = url($response_url, array('absolute' => TRUE));
          $method = 'POST';
          $room = $script->evalString($cmd->getParam('room'));
          $muted =  $script->evalString($cmd->getParam('muted', false));
          $muted =  $muted? 'true':'false';
          //TODO: Check about this param, doc is not consistent in Plivo
          $beep =  'true'; // by default, announce new participants
          $start_on_enter =  'true';
          $end_on_exit =  'false';
          //TODO: waitSound in Plivo must return a RESTXML with Play and/or Wait Elements only
          $wait_url = '';
          $hangup_on_star =  $script->evalString($cmd->getParam('hangup_on_star', false));
          $hangup_on_star =  $hangup_on_star? 'true':'false';
          $time_limit = '14400'; // max allowed for Plivo calls
          /*$response .= '<Dial action="' . $action
                            . '" method="' . $method
                            . '" hangupOnStar="' . $hangup_on_star
                            . '" timeLimit="' . $time_limit
                            . '" >';*/
          $response .= '<Conference muted="' . $muted
                            . '" beep="' . $beep
                            . '" startConferenceOnEnter="' . $start_on_enter
                            . '" endConferenceOnExit="' . $end_on_exit
                            . '" waitSound="' . $wait_url
                            . '" hangupOnStar="' . $hangup_on_star
                            . '" timeLimit="' . $time_limit
                            . '" callbackUrl="' . $action
                            . '" callbackMethod="' . $method
                            . '" >';
          $response .= $room;
          $response .= '</Conference>';
         // $response .= '</Dial>';
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdLabel':
          // don't do anything. Labels are just placeholders that mark a position within the script.
          break;

        case 'VoipCmdLog':
          $input = $script->evalString($cmd->getParam('input', ''));
          $name = $script->evalString($cmd->getParam('name', NULL));
          $script->runLog($input, $name);
          break;

        case 'VoipCmdRecord':
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/plivo/callhandler';
          $cid = $call->getCid();
          $format = $script->evalString($cmd->getParam('format'));
          if($format == 'mp3') {
            $response_url = $menu_path . '/process_record_mp3/' . $cid;
          }
          else {
            // return wav file
            $response_url = $menu_path . '/process_record/' . $cid;
          }
          $action = url($response_url, array('absolute' => TRUE));
          $method = 'POST';
          $prompt = $cmd->getParam('prompt', '');
          $timeout = $script->evalString($cmd->getParam('timeout', 5));
          $end_key = $script->evalString($cmd->getParam('end_key', '#'));
          $max_length = $script->evalString($cmd->getParam('max_length', 3600));
// TODO: implement the "transcribe" feature?"
          $play_beep = $script->evalString($cmd->getParam('play_beep'));
          $play_beep = $play_beep ? 'true' : 'false';
          if($prompt){
            $response .= _voipplivo_generate_audio_response($script, $prompt);
          }
//TODO: Check what the Plivo returns in response? Documentation mentiones only support for saving file to local filesystem?          
          $response .= '<Record action="' . $action
                            . '" method="' . $method
                            . '" timeout="' . $timeout
                            . '" finishOnKey="' . $end_key
                            . '" maxLength="' . $max_length
                            . '" playBeep="' . $play_beep
                            . '" />';
          // in case of empty recording...
          $response_url = $menu_path . '/process_empty_recording/' . $cid;
          $action = url($response_url, array('absolute' => TRUE));
          $response .= '<Redirect method="GET">'
                         . $action
                         . '</Redirect>';
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdReject':
          $call->setHangupReason(VoipCall::HANGUP_REJECTED);
          $reason_tmp = $script->evalString($cmd->getParam('reason'));
          $reason =
            ($reason_tmp == VoipScript::REJECT_BUSY)?'busy':'rejected';
          $response .= "<Hangup reason='$reason' />";
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdReturn':
          $rc = $script->runReturn();
          if($script->isError()){
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voipplivo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            // report the error to the phone user
            $language= $script->getLanguage();
            $voice = $script->getVoice();
            $text = 'Processing error. Please contact your system administrator.';
            // translate the text
            $langcode = $language;
            $text = t($text, NULL, $langcode);
            $response .= "<Speak voice='$voice' language='$language'>$text</Speak>";
          }
          break;

        case 'VoipCmdRunIvrMenu':
          $options['prompt'] = $cmd->getParam('prompt');
          $options['input_options'] =
            $script->evalString($cmd->getParam('input_options'));
          $options['invalid_msg'] = $cmd->getParam('invalid_msg');
          $options['no_input_msg'] = $cmd->getParam('no_input_msg');
          $options['max_attempts'] =
            $script->evalString($cmd->getParam('max_attempts'));
          $options['timeout'] = $script->evalString($cmd->getParam('timeout'));
          $rc = $script->runIvrMenu($options);
          break;

        case 'VoipCmdSay':
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $prompt = $cmd->getParam('prompt');
          $loop = $script->evalString($cmd->getParam('loop'));
          $response .=
            _voipplivo_generate_audio_response($script, $prompt, $loop);
          break;

        case 'VoipCmdSendText';
          $call->setCallStatus(VoipCall::IN_PROGRESS);   
          $to = $script->evalString($cmd->getParam('to'));
          //If "to" is empty then send message to the caller
          if (!$to) {
            $to = $script->evalString($script->getVar('caller_number'));
          }
          
          $text = $script->evalString($cmd->getParam('text'));
          $network = $script->evalString($cmd->getParam('network'));
          
          if (voipscript_use_sms_framework()) {
            //Use SMS Framework to send SMS message if user enabled this option
            voipscript_send_sms_framework($to, $text);
          }
          else if(strtolower($call->getCurrentChannel()) == 'offline_text'){
            //TODO: Check about this in using with Plivo? Does it make sense?
            //Use voip_text()
            $reply = TRUE;
            $call->setCurrentChannel('text');
            voip_text($text, $call, $reply);
            $processing = TRUE;
          }
          else {
            //Else: Plivo doesn't support SMS
          }
          break;
          
        case 'VoipCmdSet';
          $var_name = $script->evalString($cmd->getParam('var_name'));
          $var_value = $script->evalString($cmd->getParam('var_value'));
          $script->setVar($var_name, $var_value);
          break;

        case 'VoipCmdSetVoice';
          $voice = $script->evalString($cmd->getParam('voice'));
          $script->setVoice($voice);
          break;

        case 'VoipCmdUnset';
          $var_name = $script->evalString($cmd->getParam('var_name'));
          $script->unsetVar($var_name);
          break;
          
        case 'VoipCmdWait';
          $time_limit = $script->evalString($cmd->getParam('time_limit'));
          $menu_path = 'voip/plivo/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_wait/' . $cid;
          $redirect_url = url($response_url, array('absolute' => TRUE));
          $response .= '<Wait length="'.$time_limit.'"/><Redirect>'.$redirect_url.'</Redirect>';  
          $processing = FALSE;
          break;  
        
      }

      // go to next command
    }
  }

  // save script for next iteration
  $call->setScript($script);
  $call->save();

  // send response back to Plivo
  if($response) {

//$response .= '<!--' . "_REQUEST: " . print_r($_REQUEST,true) .  "\n" . '-->';
//$response .= '<!--' . "voipcall: " . print_r($call,true) .  "\n" . '-->';

    _voipplivo_send_response($response);
  }

  return $rc;
}

/**
 * Send responses to Plivo
 */
function _voipplivo_send_response($response) {
  $output .= '<?xml version="1.0" encoding="UTF-8" ?>';
  $output .= '<Response>'; 
  $output .= $response;
  $output .= '</Response>';

  drupal_set_header('Content-Type: text/xml; charset=utf-8');
 watchdog('response', 'Response '.print_r($output,true));
  print $output;

  return TRUE;
}


/**
 * Update 'call status' and 'hangup reason' based on Plivo's response
 */
function voipplivo_update_call_status($call, $plivo_call_status='in-progress') {
  switch($plivo_call_status) {
    default:
      $msg = 'Invalid CallStatus: @status for call cid: @cid';
      $cid = $call->getCid();
      $params = array('@status' => $plivo_call_status, '@cid' => $cid);
      $call->report_error('voipplivo', $msg, $params);
      return NULL;
      break;
    case 'failed': // call failed, most likely due to invalid number
      $call->setCallStatus(VoipCall::INVALID);
      $call->setHangupReason(VoipCall::HANGUP_INVALID_NUMBER);
      break;
    case 'queued':
      $call->setCallStatus(VoipCall::QUEUED);
      break;
    case 'ringing': //verified
      $call->setCallStatus(VoipCall::RINGING);
      break;
    case 'in-progress':  //verified
      $call->setCallStatus(VoipCall::IN_PROGRESS);
      break;
    case 'completed':  //verified
      $call->setCallStatus(VoipCall::COMPLETED);
      $hangup_reason = $call->getHangupReason();
      if(!$hangup_reason){
        $call->setHangupReason(VoipCall::HANGUP_BY_USER);
      }
      break;
    case 'busy':
      $call->setCallStatus(VoipCall::BUSY);
      $call->setHangupReason(VoipCall::HANGUP_BUSY);
      break;
    case 'no-answer':
      $call->setCallStatus(VoipCall::NO_ANSWER);
      $call->setHangupReason(VoipCall::HANGUP_NO_ANSWER);
      break;
    case 'canceled':
      $call->setCallStatus(VoipCall::CANCELED);
      $call->setHangupReason(VoipCall::HANGUP_API_REQUEST);
      break;
  }
  return $call;
}


/**
 * Process the response to a "dial" script command
 */ 
function _voipplivo_process_dial_response($call, $request){
  $script = $call->getScript();

  // update the 'dial_sid' script variable
  $script->setVar('dial_sid', $request['DialCallSid']);

  // update the 'dial_status' script variable
  $dial_status = $request['DialCallStatus'];
  switch($dial_status) {
    default:
      $cid = $call->getCid();
      $msg = 'Invalid DialCallStatus: @status for call cid: @cid';
      $params = array('@status' => $dial_status, '@cid' => $cid);
      $call->report_error('voipplivo', $msg, $params);
      return NULL;
      break;
    case 'answered':
    case 'completed':
      $script->setVar('dial_status', VoipScript::DIAL_COMPLETED);
      break;
    case 'busy':
      $script->setVar('dial_status', VoipScript::DIAL_BUSY);
      break;
    case 'no-answer':
      $script->setVar('dial_status', VoipScript::DIAL_NO_ANSWER);
      break;
    case 'failed':
      $script->setVar('dial_status', VoipScript::DIAL_FAILED);
      break;
    case 'canceled':
      $script->setVar('dial_status', VoipScript::DIAL_CANCELED);
      break;
  }

  // update the 'dial_duration' script variable
  $duration = $request['DialCallDuration'];
  if(isset($duration)){
    $script->setVar('dial_duration', $duration);
  }

  $call->setScript($script);
  return $call;
}

/**
 * Process the response to a 'send text' script command
 */
/*function _voipplivo_process_sms_response($call, $request){
  $cid = $call->getCid();
  $sms_status_tmp = $request['SmsStatus'];
  switch($sms_status_tmp) {
    default:
      $msg = 'Invalid SmsStatus: @status for call cid: @cid';
      $params = array('@status' => $sms_status_tmp, '@cid' => $cid);
      $call->report_error('voipplivo', $msg, $params);
      return NULL;
      break;
    case 'failed':
      $msg = 'SMS sending failed for call cid: @cid. Make sure your Plivo number is sms-enabled.';
      $params = array('@cid' => $cid);
      watchdog('voipplivo', $msg, $params, WATCHDOG_WARNING);
      $text_status = VoipCall::TEXT_FAILED;
      break;
    case 'invalid':
      $msg = 'Invalid SMS parameter for call cid: @cid';
      $params = array('@cid' => $cid);
      watchdog('voipplivo', $msg, $params, WATCHDOG_WARNING);
      $text_status = VoipCall::TEXT_INVALID;
      break;
    case 'sending':
      $text_status = VoipCall::TEXT_SENDING;
      break;
    case 'received': // TODO: this is status is not documented in plivo.com. is this a plivo bug?
    case 'sent':
      $text_status = VoipCall::TEXT_SENT;
      break;
  }
  $script = $call->getScript();
  $script->setVar('send_text_status', $text_status);
  $call->setScript($script);
  return $call;
}*/

define('VOIPPlivo_URL_CHECK', '#(https?://\S+(?<![,.;?\:\!]))#');

/**
 * Generate a prompt to be played to the user
 *
 * @param $script
 *   The VoipScript instance being executed
 *
 * @param $prompt
 *   Mixed. Can be any of the following
 *   - an executable string to be processed into a prompt string, a VoipPrompt
 *     instance, or an array of VoipPrompt instances
 *   - a prompt string to played to the user
 *   - an array of VoipPrompt instances to be processed
 *
 * @param $loop
 *   Integer with the number of times to repeat the prompt
 *
 * @return
 *   The prompt as a PlivoML sequence to be sent to Plivo
 */
function _voipplivo_generate_audio_response($script, $prompt, $loop=1) {
  if(is_string($prompt) && (substr($prompt, 0, 1) == '^')){
    $prompt = $script->evalString($prompt);
  }

  if(is_string($prompt)){
    $prompt_list[] = new VoipPrompt($prompt);
  }
  else if (is_object($prompt)){
    $prompt_list[] = $prompt;
  }
  else {
    $prompt_list = $prompt;
  }

  foreach($prompt_list as $p) {
    $text = $script->evalString($p->getText());
    $voice_id = $p->getVoice();
    $voice = VoipVoice::getVoice($voice_id);
    $language = $voice->getLanguage();
    $as_tmp = $p->getAs() ? $script->evalString($p->getAs()) : '';
    $as = $as_tmp ? $as_tmp : 'number';

    // split the text into audio URLs and text elements
    $elements =
      preg_split(VOIPPlivo_URL_CHECK, $text, -1, PREG_SPLIT_DELIM_CAPTURE);
  
    // process each element
    $url_flag = FALSE;
    foreach($elements as $element) {
      $element = trim($element);
      if($element) {
        if(_voipplivo_is_url($element)){
          $response .= "<Play>$element</Play>";
          $url_flag = TRUE;
        } else {
          if($url_flag) {
            // remove punctuation associated with the previous url element
            // i.e. the '!' in $text = 'Hello $audio_name!"
            $element = ltrim($element,'?!.;,:');
            $url_flag = FALSE;
          }
          if($element) {
            if($as == 'digits'){
              //Convert numbers into digits
              $element = _voipplivo_parse_numbers($element);
            }
            $response .= "<Speak voice='$voice_id' language='$language'>$element</Speak>";
          }
        }
      }
    }
  }
  
  // process the $loop parameter
  $counter = $loop - 1;
  while($counter > 0){
    $response .= $response;
    $counter--;
  }

  return $response;
}


/**
 * Check if the given string is a url
 */
function _voipplivo_is_url($str) {
  // Note: Although the checking performed is not as complete as
  //       http://stackoverflow.com/questions/161738/what-is-the-best-regular-expression-to-check-if-a-string-is-a-valid-url , it seems to serve our purposes!
  $rc = preg_match(VOIPPlivo_URL_CHECK, $str);
  return $rc;
}

function _voipplivo_parse_numbers($str){
//preg_match_all("{(\d+)}", $str, $result, PREG_PATTERN_ORDER);
//return $result;
return preg_replace_callback('{(\d+)}', '_voipplivo_numbers_add_space', $str);
}

function _voipplivo_numbers_add_space($matches) {
  $string_array = str_split($matches[0]);
  $number_with_space = implode(" ",$string_array);
  return $number_with_space;
}

function _voipplivo_transfer_file($url, $validators=array()) {
    $filename = rawurldecode(basename($url));
    $filepath = file_create_filename($filename, file_directory_temp());
    
    // Then make the actual request to download the file.
    $data = file_get_contents($url); 
    
    //Save it to temporary file.
    if ($fp = @fopen($filepath, 'w+')) {
      fputs($fp, $data); 
      fclose($fp);
    }
    
    //Save it to default files directory.
    $destination=file_directory_path();
    $file = field_file_save_file($filepath, $validators, $destination);
    // Delete the temporary file.
    @unlink($filepath);
    
    if ($file) {
      return $file;
    }
    
    return NULL;
}

function _voipplivo_validate_request() {
  $server = VoipServer::getServer('plivo');
  $server_config = $server->getConfig();
  $account_sid = $server_config['voipplivo_account_sid'];
  $auth_token = $server_config['voipplivo_auth_token'];
  // Create a new PlivoUtils object
  $utils = new PlivoUtils($account_sid, $auth_token);
  
  // Note, that if your URL uses an implied "index" document
    // (index.php), then apache often adds a slash to the SCRIPT_URI
    // while Plivo's original request will not have a slash
    // Example: if Plivo requested http://mycompany.com/answer
    //   and that url is handled by an index.php script
    //   Apache/PHP will report the URI as being:
    //   http://mycompany.com/answer/
    //   But the hash should be calculated without the trailing slash

    // Also note, if you're using URL rewriting, then you should check
    // to see that PHP is reporting your SCRIPT_URI and
    // QUERY_STRING correctly.

   if($_SERVER['HTTPS']) {
     $http = "https://";
   }
   else {
     $http = "http://";
   }  
  
  $url = $http.$_SERVER['HTTP_HOST'].$_SERVER['REQUEST_URI'];

  if(isset($_POST)) {
    // copy the post data
    $data = $_POST;
  }

  $expected_signature = $_SERVER["HTTP_X_PLIVO_SIGNATURE"];
//watchdog('voipplivo', "VALIDAITION S=$expected_signature URL=$url TOKEN=$auth_token SID:$account_sid SIG:".$utils->validateRequest($expected_signature, $url, $data), $vars, WATCHDOG_INFO);
  if($utils->validateRequest($expected_signature, $url, $data)) {
    return TRUE;
  }
  return FALSE;
}
