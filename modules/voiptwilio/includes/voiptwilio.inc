<?php
// $id$

/**
 * @file
 * Implementation of Twilio-related functionality
 */

foreach (array('twilio.php') as $file) {
  require_once(dirname(__FILE__) . DIRECTORY_SEPARATOR . $file); 
}


/**
 * Internal functions
 */

/*
 * Process incoming requests and responses from Twilio
 *
 * @param string $twilio_action set to the action to be executed. 
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
function _voiptwilio_call_handler($twilio_action, $cid=NULL) {

$msg = "in voiptwilio_call_handler() with twilio_action: $twilio_action, cid: $cid, and _REQUEST: " . print_r($_REQUEST, TRUE);
$vars = array();
watchdog('voiptwilio', $msg, $vars, WATCHDOG_INFO);
  // validate $_REQUEST['AccountSid'] to avoid hack attempts
  $server = VoipServer::getServer('twilio');
  $server_config = $server->getConfig();
  $account_sid = $server_config['voiptwilio_account_sid'];
  $incoming_sid = $_REQUEST['AccountSid'];
  if ($account_sid != $incoming_sid) {
     $msg = '_voip_twilio_call_handler called with invalid AccountSid: @sid';
     $params = array('@sid' => $incoming_sid);
     watchdog('voiptwilio', $msg, $params, WATCHDOG_WARNING);
     return FALSE;  // TODO: should we return an error response to the server?
  }
  if ($twilio_action == 'process_inbound_text') {
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
  else {
    $channel = 'voice';
    $network = 'voip';
    $from = $_REQUEST['Caller'];
    $to = $_REQUEST['Called'];
  }

  if (!$cid) {
    $voip_session = VoipSession::getSessionByNumber($from, $to);
    if ($voip_session) {
      $cid = $voip_session->getCid();
      // load the variables already associated with the call, if any
      $call = VoipCall::load($cid);
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
    if ($voip_session) {
      $voip_session->resetTimer();
      $voip_session->save();
    }
    else if ($from && $to && $twilio_action!='process_hangup'){
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
  if($twilio_action == 'process_inbound_calls') {

    $call = new VoipCall(); //TODO: Why was this restored?
    $call_sid = $_REQUEST['CallSid']; // unique identifier of the call, 34 char
    $call->setCallId($call_sid);
    $call->setSource('twilio');
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
  else if ($twilio_action == 'process_inbound_text') {
    $call_sid = $_REQUEST['SmsSid']; // unique identifier of the text, 34 char
    $call->setCallId($call_sid);
    $call->setSource('twilio');
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
  }
  else {
    // handle ongoing calls
    // 'process_hangup' for incoming calls does not come with a $cid
    if($twilio_action == 'process_hangup') {
      if(!$cid) {
        $call_id = $_REQUEST['CallSid'];
        $cid = VoipCall::getCidFromCallId($call_id);
      }
    }

    // load the variables already associated with the call, if any
    $call = VoipCall::load($cid);

    // handle first response to an outbound call
    if($twilio_action == 'process_outbound_calls') {
      // store twilio call id for all outbound calls
      $call_id = $_REQUEST['CallSid'];
      $call->setCallId($call_id);
      $answered_by = $_REQUEST['AnsweredBy'];
      if($answered_by) {
        $tmp = ($answered_by == 'human') ? VoipCall::ANSWERED_HUMAN
                                         : VoipCall::ANSWERED_MACHINE; 
        $call->setAnsweredBy($tmp);
      }
    }
  }
  
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
    $call->setCallerNumber($_REQUEST['Caller']);
  if(!$call->getCallerName())
    $call->setCallerName($_REQUEST['CallerName']);
  if(!$call->getDestNumber())
    $call->setDestNumber($_REQUEST['Called']);
  
  $twilio_call_status = $_REQUEST['CallStatus']; 
  if (empty($twilio_call_status)) {
    $twilio_call_status = $_REQUEST['SmsStatus']; 
  }
 
  // set start time for the call
  if($twilio_call_status == 'in-progress'){
    $start_time = $call->getStartTime();
    if(!$start_time) {
      // start counting the call duration
      $call->setStartTime();
    }
  }

//watchdog('voiptwilio', "twilio status: $twilio_call_status, call status: " . $call->getCallStatus());

  // update call status
  // NOTE: sms callbacks do not have a call status associated with them
  // NOTE: when the user hangs up during a recording, Twilio sends the record 
  // response with the recording data AND CallStatus set to 'completed'. Once that is done,
  // Twilio calls our process_hangup url to inform, once again, that the call has been hanged.
  
  if ($twilio_action == 'process_sms_callback'
      || $twilio_action == 'process_inbound_text'
      || $twilio_action == 'process_send_sms'
      || $twilio_action == 'process_record'
      || $twilio_action == 'process_record_mp3'
      || $twilio_action == 'process_empty_recording'
  ){
    $call->SetCallStatus(VoipCall::IN_PROGRESS);
  }
  else{
    $call = voiptwilio_update_call_status($call, $twilio_call_status);
  }

  // update script variables with call location, if available
  if(isset($_REQUEST['FromCity'])){
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
  }

  $is_hangup = $call->isHangup();


//watchdog('voiptwilio', "twilio status: $twilio_call_status, updated call status: " . $call->getCallStatus() . ", is_hangup: $is_hangup");

  // process responses to requests previously-issued by the call script
  if(!$is_hangup) {
    switch($twilio_action) {
      default:
        $msg = 'Invalid twilio_action: @action';
        $params = array('action' => $twilio_action);
        $call->report_error('voiptwilio', $msg, $params);
        return FALSE;
        break;
      case 'process_inbound_calls':
      case 'process_outbound_calls':
      case 'process_hangup':
      case 'process_inbound_text':
      case 'process_wait':
        // nothing special to be done here. the above commands have already
        // been processed in the previous lines
        break;
      case 'process_conference':
        // NOTE: in Twilio, conference calls are executed via the dial command
        //       and, therefore, should be processed in the same way
      case 'process_dial':
        $call = _voiptwilio_process_dial_response($call, $_REQUEST);
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
        $suffix = ($twilio_action == 'process_record')? '.wav' : '.mp3';
        $script = $call->getScript();
        $twilio_record_url = $_REQUEST['RecordingUrl'] . $suffix;
        //Transfer the file to local server
        $file = _voiptwilio_transfer_file($twilio_record_url);     
        $script->setVar('recording_path', file_create_url($file['filepath']));
        $script->setVar('recording_public_url', $twilio_record_url);
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

      case 'process_send_sms':
      case 'process_sms_callback':
        $call = _voiptwilio_process_sms_response($call, $_REQUEST);
        if(strtolower($call->getCurrentChannel()) == 'text') {
          // update sms status
          $call->save();
          // since 'process_sms_callback' is asynchronous, go back
          if($twilio_action == 'process_sms_callback') {
            return;
          }
        }
        break;
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
     // get call duration from Twilio
     $duration = isset($_REQUEST['CallDuration'])?$_REQUEST['CallDuration']:NULL;
    }
    $call->setDuration($duration);
  }

  // update the voipcall with the new values
  $call = $call->save();

  if ($twilio_action == 'process_inbound_text') {
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
  }
  // process the call script
  $rc = _voiptwilio_script_handler($call);

  return;  // NOTE: this function MUST NOT return any value.  If it does, the
           //       output to Twilio will get messed up with Drupal hmtl...
}

/**
 * Process voipcall scripts
 *
 * @param $call, VoipCall instance with the call information
 *
 * @return boolean.  TRUE on success or FALSE on failure.  Check voip_error() for processing failure.
 */
function _voiptwilio_script_handler($call) {
$msg = 'Entering script_handler with voipcall: ' . print_r($call,true);
watchdog('voiptwilio', $msg, array(), WATCHDOG_INFO);

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
        $call->report_error('voiptwilio', $msg, $params);
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
        watchdog('voiptwilio', $msg, $params, WATCHDOG_WARNING);
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
          $call->report_error('voiptwilio', $msg, $params);
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
            $menu_path = 'voip/twilio/callhandler';
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
                            . '" timeout="' . $timeout
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
          $menu_path = 'voip/twilio/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_get_input/' . $cid;
          $action = url($response_url, array('absolute' => TRUE));
          $method = 'POST';
          $timeout = $script->evalString($cmd->getParam('timeout', 5)); 
          $end_key = $script->evalString($cmd->getParam('end_key', '')); 
          $num_digits = $script->evalString($cmd->getParam('num_digits')); 
          $prompt = $cmd->getParam('prompt'); 
          $response .= '<Gather action="' . $action
                            . '" method="' . $method
                            . '" timeout="' . $timeout
                            . '" finishOnKey="' . $end_key;
          if($num_digits) {
            $response .=      '" numDigits="' . $num_digits;
          }
          $response .= '" >';
          if($prompt) {
            $response .=  _voiptwilio_generate_audio_response($script, $prompt);
          }
          $response .= '</Gather>';
          // in case of timeout or empty input, set Digits to 'no_input'
          $action_timeout = $action . '&amp;Digits=' . 'no_input';
          $response .= '<Redirect method="GET">'
                         . $action_timeout
                         . '</Redirect>';
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
            $call->report_error('voiptwilio', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            $text = 'Processing error. Please contact your system administrator.';        
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $response .= "<Say>$text</Say>";
          }
          break;

        case 'VoipCmdGoto';
          $label_name = $script->evalString($cmd->getParam('label_name', ''));
          $rc = $script->runGoto($label_name);
          if($script->isError()){
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voiptwilio', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            $text = 'Processing error. Please contact your system administrator.';        
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $response .= "<Say>$text</Say>";
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
              $call->report_error('voiptwilio', $msg, $params);
              $rc = FALSE;
              $processing = FALSE;
              $found = TRUE;
              $text = 'Processing error. Please contact your system administrator.';        
              $langcode = $script->getLanguage();
              $text = t($text, NULL, $langcode);
              $response .= "<Say>$text</Say>";
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
               // Note: this will trigger a response from Twilio with $twilio_action = process_hangup and CallStatus = completed
               $response .= '<Hangup/>';
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
                // Note: this will trigger a response from Twilio with $twilio_action = process_hangup and CallStatus = completed
                $response .= '<Hangup/>';
              }
            break;
          }

         
           
          $processing = FALSE;
          break;

        case 'VoipCmdJoinConference';
          // NOTE: Twilio offers several conference call parameters that are not
          //       supported by the VoIP Drupal API. In order to make those
          //       options available, the developer would have to extend the
          //       VoipScript class into something like VoipScriptTwilio.
          //       However, the specific commands of such class would not be 
          //       supported by other VoIP servers.
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/twilio/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_conference/' . $cid;
          $action = url($response_url, array('absolute' => TRUE));
          $method = 'POST';
          $room = $script->evalString($cmd->getParam('room'));
          $muted =  $script->evalString($cmd->getParam('muted', false));
          $muted =  $muted? 'true':'false';
          $beep =  'true'; // by default, announce new participants
          $start_on_enter =  'true';
          $end_on_exit =  'false';
          $wait_url = '';
          $hangup_on_star =  $script->evalString($cmd->getParam('hangup_on_star', false));
          $hangup_on_star =  $hangup_on_star? 'true':'false';
          $time_limit = '14400'; // max allowed for Twilio calls
          $response .= '<Dial action="' . $action
                            . '" method="' . $method
                            . '" hangupOnStar="' . $hangup_on_star
                            . '" timeLimit="' . $time_limit
                            . '" >';
          $response .= '<Conference muted="' . $muted
                            . '" beep="' . $beep
                            . '" startConferenceOnEnter="' . $start_on_enter
                            . '" endConferenceOnExit="' . $end_on_exit
                            . '" waitUrl="' . $wait_url
                            . '" >';
          $response .= $room;
          $response .= '</Conference>';
          $response .= '</Dial>';
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
          $menu_path = 'voip/twilio/callhandler';
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
            $response .= _voiptwilio_generate_audio_response($script, $prompt);
          }
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
          $response .= "<Reject reason='$reason' />";
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdReturn':
          $rc = $script->runReturn();
          if($script->isError()){
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voiptwilio', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            // report the error to the phone user
            
            $voice_id = $script->getVoice();
            //Twilio uses gender instead of voice id
            $voice = VoipVoice::getVoice($voice_id);
            $gender = $voice->getGender();
            $language = $voice->getLanguage();
            $text = 'Processing error. Please contact your system administrator.';
            // translate the text
            $text = t($text, NULL, $language);
            $response .= "<Say voice='$gender' language='$language'>$text</Say>";
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
            _voiptwilio_generate_audio_response($script, $prompt, $loop);
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
            //Use voip_text()
            $reply = TRUE;
            $call->setCurrentChannel('text');
            voip_text($text, $call, $reply);
            $processing = TRUE;
          }
          else {
            $menu_path = 'voip/twilio/callhandler';
            $cid = $call->getCid();
            $response_url = $menu_path . '/process_send_sms/' . $cid;
            $action = url($response_url, array('absolute' => TRUE));
            $callback_url = $menu_path . '/process_sms_callback/' . $cid;
            $callback = url($callback_url, array('absolute' => TRUE));
            $method = 'POST';
          
            // Note: If no caller number provided, Twilio will automatically try
            //       to determine an appropriate value depending on whether the
            //       call is inbound or outbound. However, since that number might
            //       not be sms capable, it might incur in a processing error.
            // Use the system's number as the default caller id
            $default_cid_number = variable_get('voipcall_cid_number', NULL);
            $caller_id = $script->evalString($cmd->getParam('caller_id',
                                                         $default_cid_number)); 
            $rc = TRUE;                                             
            switch(strtolower($network)){
              default:
                $params = array('@network' => $network,
                                '@script_name' => $script->getName(),
                                '@script' => print_r($script,true));
                $msg = "Invalid 'network' parameter '@network' for SendText command in script '@script_name': @script";
                $call->report_error('voiptwilio', $msg, $params);
                $rc = FALSE;
                $processing = FALSE;
                break;

              case 'sms':
                $response .= '<Sms action="' . $action
                             . '" statusCallback="' . $callback
                             . '" method="' . $method;
                if($to){
                  $response .= '" to="' . $to;
                }
                if($caller_id){
                  $response .= '" from="' . $caller_id;
                }
                $response .= '" >';
                $response .= $text;
                $response .= '</Sms>';
                // since the 'action' parameter prevents other commands from
                // being executed, set $processing to FALSE
                $processing = FALSE;
                break;
            } 
          }
          break;
          
        case 'VoipCmdSet';
          $var_name = $script->evalString($cmd->getParam('var_name'));
          $var_value = $script->evalString($cmd->getParam('var_value'));
          $script->setVar($var_name, $var_value);
          break;

        case 'VoipCmdSetVoice';
          $voice = $script->evalString($cmd->getParam('voice'));
          $script->setVoice($voice_id);
          break;

        case 'VoipCmdUnset';
          $var_name = $script->evalString($cmd->getParam('var_name'));
          $script->unsetVar($var_name);
          break;
          
        case 'VoipCmdWait';
          $time_limit = $script->evalString($cmd->getParam('time_limit'));
          $menu_path = 'voip/twilio/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_wait/' . $cid;
          $redirect_url = url($response_url, array('absolute' => TRUE));
          $response .= '<Pause length="'.$time_limit.'"/><Redirect>'.$redirect_url.'</Redirect>';  
          $processing = FALSE;
          break;  
        
      }

      // go to next command
    }
  }

  // save script for next iteration
  $call->setScript($script);
  $call->save();

  // send response back to Twilio
  if($response) {

//$response .= '<!--' . "_REQUEST: " . print_r($_REQUEST,true) .  "\n" . '-->';
//$response .= '<!--' . "voipcall: " . print_r($call,true) .  "\n" . '-->';

    _voiptwilio_send_response($response);
  }

  return $rc;
}

/**
 * Send responses to Twilio
 */
function _voiptwilio_send_response($response) {
  $output .= '<?xml version="1.0" encoding="UTF-8" ?>';
  $output .= '<Response>'; 
  $output .= $response;
  $output .= '</Response>';

  drupal_set_header('Content-Type: text/xml; charset=utf-8');
//TODO: watchdog('response', htmlspecialchars($output));
  print $output;

  return TRUE;
}


/**
 * Update 'call status' and 'hangup reason' based on Twilio's response
 */
function voiptwilio_update_call_status($call, $twilio_call_status='in-progress') {
  switch($twilio_call_status) {
    default:
      $msg = 'Invalid CallStatus: @status for call cid: @cid';
      $cid = $call->getCid();
      $params = array('@status' => $twilio_call_status, '@cid' => $cid);
      $call->report_error('voiptwilio', $msg, $params);
      return NULL;
      break;
    case 'failed': // call failed, most likely due to invalid number
      $call->setCallStatus(VoipCall::INVALID);
      $call->setHangupReason(VoipCall::HANGUP_INVALID_NUMBER);
      break;
    case 'queued':
      $call->setCallStatus(VoipCall::QUEUED);
      break;
    case 'ringing':
      $call->setCallStatus(VoipCall::RINGING);
      break;
    case 'in-progress':
      $call->setCallStatus(VoipCall::IN_PROGRESS);
      break;
    case 'completed':
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
function _voiptwilio_process_dial_response($call, $request){
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
      $call->report_error('voiptwilio', $msg, $params);
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
function _voiptwilio_process_sms_response($call, $request){
  $cid = $call->getCid();
  $sms_status_tmp = $request['SmsStatus'];
  switch($sms_status_tmp) {
    default:
      $msg = 'Invalid SmsStatus: @status for call cid: @cid';
      $params = array('@status' => $sms_status_tmp, '@cid' => $cid);
      $call->report_error('voiptwilio', $msg, $params);
      return NULL;
      break;
    case 'failed':
      $msg = 'SMS sending failed for call cid: @cid. Make sure your Twilio number is sms-enabled.';
      $params = array('@cid' => $cid);
      watchdog('voiptwilio', $msg, $params, WATCHDOG_WARNING);
      $text_status = VoipCall::TEXT_FAILED;
      break;
    case 'invalid':
      $msg = 'Invalid SMS parameter for call cid: @cid';
      $params = array('@cid' => $cid);
      watchdog('voiptwilio', $msg, $params, WATCHDOG_WARNING);
      $text_status = VoipCall::TEXT_INVALID;
      break;
    case 'sending':
      $text_status = VoipCall::TEXT_SENDING;
      break;
    case 'received': // TODO: this is status is not documented in twilio.com. is this a twilio bug?
    case 'sent':
      $text_status = VoipCall::TEXT_SENT;
      break;
  }
  $script = $call->getScript();
  $script->setVar('send_text_status', $text_status);
  $call->setScript($script);
  return $call;
}

define('VOIPTWILIO_URL_CHECK', '#(https?://\S+(?<![,.;?\:\!]))#');

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
 *   The prompt as a TwilioML sequence to be sent to Twilio
 */
function _voiptwilio_generate_audio_response($script, $prompt, $loop=1) {
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

  $default_voice_id = $script->getVoice();

  foreach($prompt_list as $p) {
    $text = $script->evalString($p->getText());
    $voice_id = $p->getVoice() ? $p->getVoice() : $default_voice_id;
    $voice = VoipVoice::getVoice($voice_id);
    $gender = $voice->getGender();
    $language = $voice->getLanguage();
       
    $as_tmp = $p->getAs() ? $p->getAs() : '';
    $as = $as_tmp ? $as_tmp : 'number';

    // split the text into audio URLs and text elements
    $elements =
      preg_split(VOIPTWILIO_URL_CHECK, $text, -1, PREG_SPLIT_DELIM_CAPTURE);
  
    // process each element
    $url_flag = FALSE;
    foreach($elements as $element) {
      $element = trim($element);
      if($element) {
        if(_voiptwilio_is_url($element)){
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
              $element = _voiptwilio_parse_numbers($element);
            }
            $response .= "<Say voice='$gender' language='$language'>$element</Say>";
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
function _voiptwilio_is_url($str) {
  // Note: Although the checking performed is not as complete as
  //       http://stackoverflow.com/questions/161738/what-is-the-best-regular-expression-to-check-if-a-string-is-a-valid-url , it seems to serve our purposes!
  $rc = preg_match(VOIPTWILIO_URL_CHECK, $str);
  return $rc;
}

function _voiptwilio_parse_numbers($str){
//preg_match_all("{(\d+)}", $str, $result, PREG_PATTERN_ORDER);
//return $result;
return preg_replace_callback('{(\d+)}', '_voiptwilio_numbers_add_space', $str);
}

function _voiptwilio_numbers_add_space($matches) {
  $string_array = str_split($matches[0]);
  $number_with_space = implode(" ",$string_array);
  return $number_with_space;
}

function _voiptwilio_transfer_file($url, $validators=array()) {
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
