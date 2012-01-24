<?php
// $id$

/**
 * @file
 * Implementation of tropo-related functionality
 */

/**
 * Internal functions
 */
/*
 * Process incoming requests and responses from tropo
 *
 * @param string $tropo_action set to the action to be executed. 
 *   It can assume any of the following values:
 *   - process_conference, to handle responses to conference call requests
 *   - process_inbound_calls, to handle incoming calls
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
 * @param string $cid with the id of the call
 *
 * @return void
 */
function _voiptropo_call_handler($tropo_action='', $cid=NULL, $dial_response_name='') {
  $server = VoipServer::getServer('tropo');
  $server_config = $server->getConfig();

  if ($tropo_action != 'save_record') {
    //save_record doesn't have json
    try {
      $json = json_decode(@file_get_contents('php://input')); 
      if(!$json && isset($_POST['json'])) {
         $json = json_decode($_POST['json']);
         $session = new Session($_POST['json']);
      }
      else {
        $session = new Session();
      }
     
      if (empty($tropo_action)) {
        $tropo_action = $session->getParameters('tropo_action');
        if (empty($tropo_action)) {
          $tropo_action = 'process_inbound_calls';
        }
      }
      if (empty($cid)) {
        $cid = $session->getParameters('cid');
      }
      $msg = "in voiptropo_call_handler() with tropo_action: $tropo_action, cid: $cid and Session ". print_r($json, TRUE);
      $vars = array();
      watchdog('voiptropo', $msg, $vars, WATCHDOG_INFO);
    }
    catch (TropoException $e) {
      try{
        $result = new Result();
        //watchdog('voiptropo', 'Result ' . print_r($result, TRUE) , $vars, WATCHDOG_INFO);
      }
      catch (TropoException $e) {
        watchdog('voiptropo', 'error in _voip_tropo_call_handler, Tropo reported: '. $e->getMessage(), array(), WATCHDOG_ERROR);
        return FALSE;  // TODO: should we return an error response to the server?
      }
    }
  }
  //Determine channel and network
  $channel = strtolower($json->session->to->channel);
  $network = strtolower($json->session->to->network);
 
 
 if ($tropo_action == 'process_outbound_calls') {
   $from = $session->getParameters('caller_number');
   $to = $session->getParameters('dest_number');   
 }
 else {
   $from = $json->session->from->id;
   $to = $json->session->to->id;  
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
    else if ($from && $to) {
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
    //Else?
  }
  /*
     if($tropo_action=='process_voice_analysis'){
     watchdog('voiptropo_analysis', 'RES:'.print_r($result, TRUE), $vars, WATCHDOG_INFO);
     }
   */
  //TODO: validate $_REQUEST['AccountSid'] to avoid hack attempts
  /*$account_username = $server_config['voiptropo_account_username'];
    $incoming_sid = $_REQUEST['AccountSid'];
    if ($account_sid != $incoming_sid) {
    $msg = '_voip_tropo_call_handler called with invalid AccountSid: @sid';
    $params = array('@sid' => $incoming_sid);
    watchdog('voiptropo', $msg, $params, WATCHDOG_WARNING);
    return FALSE;  // TODO: should we return an error response to the server?
    }
   */
  // handle incoming call requests
  if ($tropo_action == 'process_inbound_calls') {    
    $call_id=$session->getId(); 
    $call->setCallId($call_id);
    $call->setSource('tropo');
    $dir_tmp = VoipCall::INBOUND;
    $call->setDirection($dir_tmp);
    $call->setAnsweredBy(VoipCall::ANSWERED_DRUPAL);
    $call->setCallStatus(VoipCall::RINGING);
    $call->setStartTime(time());
    $call->setCurrentChannel($channel);
    $call->setCurrentNetwork($network);
    $call->save();
  }
  else {
    // handle ongoing calls

    // 'process_hangup' for incoming calls does not come with a $cid
    if ($tropo_action == 'process_hangup') {
      if (!$cid) {
        $call_id=$session->getId(); 
        $cid = VoipCall::getCidFromCallId($call_id);
      }
    }

    // load the variables already associated with the call, if any
    $call = VoipCall::load($cid);
    // handle first response to an outbound call
    if ($tropo_action == 'process_outbound_calls' || $tropo_action == 'process_outbound_text') {
      // store tropo call id for all outbound calls
      $call_id=$session->getId(); 
      $call->setCallId($call_id);
      
      /*if ($tropo_action == 'process_outbound_calls') {
        //TODO:
        //$answered_by = $_REQUEST['AnsweredBy'];
        if ($answered_by) {
          $tmp = ($answered_by == 'human') ? VoipCall::ANSWERED_HUMAN : VoipCall::ANSWERED_MACHINE; 
          $call->setAnsweredBy($tmp);
        }
      }*/
    }
  }
  if (in_array($channel, array('offline_text','text'))) {
    //Set text-related variables in the call object 
    $inbound_text_contents = $session->getInitialText();
    $call->setTextVariables($inbound_text_contents, $from, $to, $network);
  }
  
  // get the cid of the call data
  $cid = $call->getCid();
  if ($voip_session && !$voip_session->getCid()) {
    $voip_session->setCid($cid);
    $voip_session->save();
  }
    
  // update common call variables
  if(!$call->getCurrentChannel())
    $call->setCurrentChannel($channel);
  if(!$call->getCurrentNetwork())
    $call->setCurrentNetwork($network);
  if (!$call->getCallerNumber()) {
    $caller_number = $session->getParameters('caller_number');
    if(!$caller_number) {
      $caller_number = $json->session->from->id;
    }
    $call->setCallerNumber($caller_number);
  }
  if (!$call->getDestNumber()) {
    $dest_number = $session->getParameters('dest_number');
    if(!$dest_number) {
      $dest_number = $json->session->to->id;
    }
    $call->setDestNumber($dest_number);
  }
 
  if (!$call->getCallerName()) {
    $call->setCallerName($json->session->from->name);
  }
  
  // NOTE: sms callbacks do not have a call status associated with them
  if ($tropo_action == 'process_sms_callback' || $tropo_action == 'process_outbound_text') {
    //TODO: Should we move sms callback to another menu callback?
    $call->SetCallStatus(VoipCall::IN_PROGRESS);
  }

  $is_hangup = $call->isHangup();
  // process responses to requests previously-issued by the call script
  if (!$is_hangup) {
    switch ($tropo_action) {
      default:
        $msg = 'Invalid tropo_action: @action';
        $params = array('action' => $tropo_action);
        $call->report_error('voiptropo', $msg, $params);
        return FALSE;
        break;
      case 'process_outbound_calls':
        $tropo_call_status='in-progress';
        voiptropo_update_call_status($call, $tropo_call_status); 
        $start_time = $call->getStartTime();
        if (!$start_time) {
          // start counting the call duration
          $call->setStartTime();
        }
        $call = $call->save();
        break;
      case 'process_inbound_calls':
      case 'process_outbound_text':
      case 'process_wait':
        // nothing special to be done here. the above commands have already
        // been processed in the previous lines
        break;
      case 'process_hangup':
        $call->SetCallStatus(VoipCall::COMPLETED);
        $call->setHangupReason(VoipCall::HANGUP_BY_USER);
        break;
      case 'process_dial':
      case 'process_conference':
        $call = _voiptropo_process_dial_response($call, $result, $dial_response_name);
        break;
      case 'process_callstatus':
        voiptropo_update_call_status($call, $dial_response_name, $json);
        // determine call duration
        $end_time = $call->setEndTime();
        $start_time = $call->getStartTime();
        if ($start_time) {
          $duration  = $end_time - $start_time;
        }
        else {
          //get call duration from tropo
          $duration = isset($json->result->sessionDuration ) ? $json->result->sessionDuration : NULL;
        }
        $call->setDuration($duration);
        $call->save();
        return;
        break;  
      case 'process_get_input':
        $script = $call->getScript();
        $digits = str_replace(" ","",$result->getValue());
        $script->setVar('input_digits', $digits);
        $call->setScript($script);
        break;
     case 'process_get_voice_input':
        $script = $call->getScript();
        $script->setVar('input_digits', $result->getValue());
        $call->setScript($script);
        break;
      case 'save_record':
        $validators = array();
        $filename=$_FILES['filename']['name'];
        $dest = file_directory_path();
        $file = _voiptropo_file_save_upload($filename, $validators, $dest);
        $script = $call->getScript();
        $script->setVar('recording_path', file_create_url($file->filepath));
        $script->setVar('recording_public_url', file_create_url($file->filepath));
        $script->setVar('recording_digits', NULL);
        $script->setVar('recording_fid', $file->fid);
        $call->setScript($script);
        // update the voipcall with the new values
        $call = $call->save();
        return;
        break;
      case 'process_record':
      case 'process_record_hangup':
        //Save record duration
        $rec_actions = $result->getActions();
        $script = $call->getScript();
        $script->setVar('recording_duration', $rec_actions->duration);
        if($tropo_action) {
          $digits = 'hangup';
        }
        $script->setVar('recording_digits', $digits);
        $call->setScript($script);
        // update the voipcall with the new values
        $call = $call->save();
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
        $call = _voiptropo_process_sms_response($call, $dial_response_name);
        break;
    }
  }

  if ($is_hangup) {
    // determine call duration
    $end_time = $call->setEndTime();
    $start_time = $call->getStartTime();
    if ($start_time) {
      $duration  = $end_time - $start_time;
    }
    else {
      //TODO: get call duration from tropo
      $duration = isset($json->result->actions->duration ) ? $json->result->actions->duration : NULL;
    }
    $call->setDuration($duration);
  }

  // update the voipcall with the new values
  $call = $call->save();
  $menu_path = 'voip/tropo/callhandler';
  $tropo = new Tropo();

  if ($tropo_action == 'process_outbound_calls') {
    //Make outbound call
    $dest_number =$call->getDestNumber();
    $params = array(
        'from' => $call->getCallerNumber(),
        'allowSignals' => 'hangup',
        );
    $tropo->call($dest_number, $params);

    //Add events for outbound dial.
    $response_url = $menu_path . '/process_hangup/' . $cid ;
    $action = url($response_url, array('absolute' => TRUE));
    //$tropo->on(array("event" => "hangup", "next" => $action));

    $response_url = $menu_path . '/process_callstatus/' . $cid;
    $action = url($response_url . '/error', array('absolute' => TRUE));
    $tropo->on(array("event" => "error", "next" => $action));

    $action = url($response_url . '/incomplete', array('absolute' => TRUE));
    $tropo->on(array("event" => "incomplete", "next" => $action));


    // $response_url = $menu_path . '/process_recorded_test/' . $cid;
    //$action = url($response_url, array('absolute' => TRUE));
    /* $record_params = array(
       'say' =>'start',
       'beep' => 'false',
    //'timeout' => '10.0',
    'maxSilence' => '1.0',
    'maxTime' => '10.0',
    //'url' => $action,
    );

    $tropo->record($record_params);

    $response_url = $menu_path . '/process_voice_analysis/' . $cid;
    $action = url($response_url, array('absolute' => TRUE));
    $tropo->on(array("event" => "continue", "next" => $action)); 
     */
  }
  else if($tropo_action == 'process_outbound_text') {
   $to = $call->getDestNumber();   
   $text = $session->getParameters('text');
   $params = array(
     'to' => $to,
     'channel' => 'TEXT',
     'network' => 'SMS',
    );

    $tropo->message($text, $params);
  }
  
  if ($tropo_action == 'process_inbound_calls' && in_array($call->getCurrentChannel(), array('offline_text','text'))) {
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
  $rc = _voiptropo_script_handler($call, $tropo);
  
  //Add final event
  if ($tropo_action != 'process_callstatus' && $channel != 'text') {
    $response_url = $menu_path . '/process_callstatus/' . $cid . '/continue' ;
    $action = url($response_url, array('absolute' => TRUE));
    $tropo->on(array("event" => "continue", "next" => $action));
  }

  // send response back to tropo
  $tropo->renderJSON();

  return;  // NOTE: this function MUST NOT return any value.  If it does, the
  //       output to tropo will get messed up with Drupal hmtl...
}

/**
 * Process voipcall scripts
 *
 * @param $call, VoipCall instance with the call information
 *
 * @return boolean.  TRUE on success or FALSE on failure.  Check voip_error() for processing failure.
 */
function _voiptropo_script_handler($call, &$tropo) {
  $msg = 'Entering script_handler with voipcall: ' . print_r($call, TRUE);
  watchdog('voiptropo', $msg, array(), WATCHDOG_INFO);

  $rc = TRUE;

  // get the script object associated with the call
  $script = $call->getScript();
  // check if call is in a hangup state
  $hangup = $call->isHangup();
  if ($hangup) {
    $hangup_script_name = $call->getHangupScriptName();
    // Stop processing if no hangup script defined
    if (!$hangup_script_name) {
      return TRUE;
    }
    else {
      // load hangup script commands
      $rc = $script->runGosub($hangup_script_name);
      if ($script->isError()) {
        $msg = $script->getErrorMessage();
        $params = array();
        $call->report_error('voiptropo', $msg, $params);
        return FALSE;
      }
    }
  }
  
  $processing = TRUE;
  $response = '';

  //Set script language
  $language= $script->getLanguage();
  $voice = $script->getVoice();
  $tropo->setVoice($voice);

  while ($processing) {

    $cmd = $script->getNextCommand();
    if (!$cmd) {
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
        $tropo->hangup();
        $msg = 'Script @name ended without a "hangup" command.';
        $params = array('@name' => $script->getName());
        watchdog('voiptropo', $msg, $params, WATCHDOG_WARNING);
        $call->setHangupReason(VoipCall::HANGUP_SCRIPT_END);
      }
      //for text channel don't do anything
    }
    // process current command
    else {
      $cmd_id = $cmd->getId();

      switch ($cmd_id) {
        default:
          $msg = "Invalid command id '@cmd_id' in script '@script_name': @script";
          $params = array('@cmd_id' => $cmd_id, '@script_name' => $script->getName(), '@script' => print_r($script, TRUE));
          $call->report_error('voiptropo', $msg, $params);
          $rc = FALSE;
          $processing = FALSE;
          break;

        case 'VoipCmdDial';
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $number = $script->evalString($cmd->getParam('number'));
          $number = str_replace(" ", "", $number); //remove any spaces from number
          if ($call->getCurrentChannel() == 'text'  || $call->getCurrentChannel() == 'offline_text') {
            //In case VoipCmdDial is called within text channel then instead of transfer we call the sender.
            //we are starting a call, hangup() the existing session
            $tropo->hangup();
            
            if (!$number) {
              $number = $call->getCallerNumber();
            }
            //$call = new VoipCall();
            $call->setDestNumber($number);
            $call->setScript($script);
            $call->save();
            voip_dial($call);
          }
          else {
            $menu_path = 'voip/tropo/callhandler';
            $cid = $call->getCid();
            $timeout = $script->evalString($cmd->getParam('timeout', 30));
            //TODO: What command for timeLimit, hangupOnStar?
            /*$hangup_on_star =  $script->evalString($cmd->getParam('hangup_on_star', false));
            $hangup_on_star =  $hangup_on_star? 'true':'false';
            $time_limit = $script->evalString($cmd->getParam('time_limit', '14400'));*/
            $caller_number = $script->evalString($script->getVar('caller_number'));
            $caller_id = $script->evalString($cmd->getParam('caller_id', $caller_number)); 

            $params = array(
                //'timeout' => $timeout,  //TODO: Timeout causes immediate hangup, disabled for now.
                'from' => $caller_id,
            );
            $tropo->transfer($number, $params);

            //Add events
            $response_url = $menu_path . '/process_dial/' . $cid . '/error';
            $action = url($response_url, array('absolute' => TRUE));
            $tropo->on(array("event" => "error", "next" => $action));

            $response_url = $menu_path . '/process_dial/' . $cid . '/incomplete';
            $action = url($response_url, array('absolute' => TRUE));
            $tropo->on(array("event" => "incomplete", "next" => $action));
          }
         
          $rc = TRUE;
          $processing = FALSE;
          break;
 
        case 'VoipCmdGetInput':
          _voiptropo_get_input('dtmf', $call, $cmd, $script, $tropo);
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdGetVoiceInput':
          _voiptropo_get_input('voice', $call, $cmd, $script, $tropo);
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdGosub':
          $script_name = $script->evalString($cmd->getParam('script_name'));
          $options = $script->evalString($cmd->getParam('options'));
          $rc = $script->runGosub($script_name, $options);
          if ($script->isError()) {
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voiptropo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            $text = 'Processing error. Please contact your system administrator.';
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $tropo->say($text);
          }
          break;

          case 'VoipCmdGoto';
          $label_name = $script->evalString($cmd->getParam('label_name', ''));
          $rc = $script->runGoto($label_name);
          if ($script->isError()) {
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voiptropo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            $text = 'Processing error. Please contact your system administrator.';
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $tropo->say($text);
          }
          break;

          case 'VoipCmdGotoIf';
          $condition = $script->evalString($cmd->getParam('condition', ''));
          if ($condition) { 
            $label_name = $script->evalString($cmd->getParam('label_name', ''));
            $rc = $script->runGoto($label_name, $condition);
            if ($script->isError()) {
              $msg = $script->getErrorMessage();
              $params = array();
              $call->report_error('voiptropo', $msg, $params);
              $rc = FALSE;
              $processing = FALSE;
              $found = TRUE;
              $text = 'Processing error. Please contact your system administrator.';
              $langcode = $script->getLanguage();
              $text = t($text, NULL, $langcode);
              $tropo->say($text);
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
          
          if ($call->getCurrentChannel() != 'text') {
            // send the hangup request and stop processing script commands
            $tropo->hangup();
          }
          
          $processing = FALSE;
          break;

          case 'VoipCmdJoinConference';
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_conference/' . $cid;
          $action = url($response_url, array('absolute' => TRUE));

          $room = $script->evalString($cmd->getParam('room'));
          $muted =  $script->evalString($cmd->getParam('muted', FALSE));
          $muted =  $muted? 'true':'false';
          /*$beep =  $script->evalString($cmd->getParam('beep', false));
            $beep =  $beep? 'true':'false';
            $start_on_enter =  $script->evalString($cmd->getParam('start_on_enter', false));
            $start_on_enter =  $start_on_enter? 'true':'false';
            $end_on_exit =  $script->evalString($cmd->getParam('end_on_exit', false));
            $end_on_exit =  $end_on_exit? 'true':'false';*/
          $wait_url =  $script->evalString($cmd->getParam('wait_url', ''));
          $hangup_on_star =  $script->evalString($cmd->getParam('hangup_on_star', FALSE));
          //$hangup_on_star =  $hangup_on_star? 'true':'false';
          if ($hangup_on_star) {
            $terminator="*";
          }
          else{
            $terminator="";
          }
          //$time_limit = $script->evalString($cmd->getParam('time_limit', '3600'));

          //TODO: What about this options: beep, start_on_enter, end_on_exit, time_limit?
          $params = array(
              'id' => $room,
              'mute' => $muted,
              'terminator' => $terminator,
              );

          $tropo->conference($room, $params);
          $tropo->on(array("event" => "continue", "next" => $action));

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
          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();
          $format = $script->evalString($cmd->getParam('format'));
          if ($format == 'mp3') {
            $record_format = 'audio/mp3';
          }
          else {
            // return wav file        
            $record_format = 'audio/wav';
          }
          $response_url = $menu_path . '/save_record/' . $cid;
          $record_action = url($response_url, array('absolute' => TRUE));

          $response_url = $menu_path . '/process_record/' . $cid;
          $success_action = url($response_url, array('absolute' => TRUE));
          $method = 'POST';
          $timeout = $script->evalString($cmd->getParam('timeout', 5.0));
          $end_key = $script->evalString($cmd->getParam('end_key', '#'));
          $max_length = $script->evalString($cmd->getParam('max_length', 3600));

          $play_beep = $script->evalString($cmd->getParam('play_beep'));
          $play_beep = $play_beep ? 'true' : 'false';
          $response_url = $menu_path . '/process_empty_recording/' . $cid;
          $timeout_action = url($response_url, array('absolute' => TRUE));

          //$choices->terminator=$end_key;
          $choices=$end_key;
          $params = array(
              'maxTime' => $max_length,
              'maxSilence' => (float)$timeout,
              'beep' => $play_beep,
              'format' => $record_format,
              'method' => $method,
              'url' => $record_action, 
              'choices' => $choices,
              );

          $prompt = _voiptropo_prompt_validate($script, $cmd->getParam('prompt'));
          $default_language = $script->getLanguage();
          $default_voice = $script->getVoice();

          /*bug with Tropo in VoipCmdRecord. If say parameter is missing from tropo record() command, an error will happen terminating 
            the current call. Therefore getting last item in array and moving it to record() command.*/
          $last_say = array_pop($prompt);
          $text = $script->evalString($last_say->getText());
          $text = _voiptropo_strip_special_chars($text);
          $voice_id = $last_say->getVoice() ? $last_say->getVoice() :  $default_voice;
          $as = $last_say->getAs() ? $script->evalString($last_say->getAs()) : '';

          if (!empty($as) && in_array($as, array('digits', 'date'))) {
            $params['as'] = $as; 
          }
          $params['voice'] = $voice_id;
          $params['say'] = $text;

          foreach ($prompt as $p) {
            $text = $script->evalString($p->getText());
            $text = _voiptropo_strip_special_chars($text);
            $voice_id = $p->getVoice() ? $p->getVoice() : $default_voice;
            $as = $p->getAs() ? $script->evalString($p->getAs()) : '';
            $say_params = array();
            if (!empty($as) && in_array($as, array('digits', 'date'))) {
              $say_params['as'] = $as; 
            }
            $say_params['voice'] = $voice_id;
            $tropo->say($text, $say_params);
          }


          $tropo->record($params);
          //What next:
          $tropo->on(array("event" => "continue", "next" => $success_action));
          // in case of empty recording...
          $tropo->on(array("event" => "incomplete", "next" => $timeout_action));
          $response_url = $menu_path . '/process_record_hangup/' . $cid;      
          $hangup_action = url($response_url, array('absolute' => TRUE));
          $tropo->on(array("event" => "hangup", "next" => $hangup_action));

          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdReject':
          $call->setHangupReason(VoipCall::HANGUP_REJECTED);
          //TODO: Is there reason param in Tropo?
          $tropo->reject();
          $rc = TRUE;
          $processing = FALSE;
          break;

        case 'VoipCmdReturn':
          $rc = $script->runReturn();
          if ($script->isError()) {
            $msg = $script->getErrorMessage();
            $params = array();
            $call->report_error('voiptropo', $msg, $params);
            $rc = FALSE;
            $processing = FALSE;
            $found = TRUE;
            // report the error to the phone user
            $text = 'Processing error. Please contact your system administrator.';
            // translate the text
            $langcode = $script->getLanguage();
            $text = t($text, NULL, $langcode);
            $tropo->say($text);
          }
          break;

        case 'VoipCmdRunIvrMenu':
          $options['prompt'] = $cmd->getParam('prompt');
          $options['input_options'] = $script->evalString($cmd->getParam('input_options'));
          $options['invalid_msg'] = $cmd->getParam('invalid_msg');
          $options['no_input_msg'] = $cmd->getParam('no_input_msg');
          $options['max_attempts'] = $script->evalString($cmd->getParam('max_attempts'));
          $options['timeout'] = $script->evalString($cmd->getParam('timeout'));
          $rc = $script->runIvrMenu($options);
          break;  

        case 'VoipCmdSay':
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $loop = $script->evalString($cmd->getParam('loop'));
          $prompt = _voiptropo_prompt_validate($script, $cmd->getParam('prompt'));
          $default_language = $script->getLanguage();
          $default_voice = $script->getVoice();

          for ($i=0;$i<$loop;$i++) {              
            foreach ($prompt as $p) {
              $text = $script->evalString($p->getText());
              $text = _voiptropo_strip_special_chars($text);
              $voice_id = $p->getVoice() ? $p->getVoice() : $default_voice;
//watchdog("debug", "Say: voice: $voice_id, language: $language, text: $text");
              $as = $p->getAs() ? $script->evalString($p->getAs()) : '';
              $say_params = array();
              if($as == 'digits'){
                $text = "<speak>"._voiptropo_parse_numbers($text)."</speak>";
              }
              elseif($as == 'date'){
                $say_params['as'] = $as; 
              }
              
              $say_params['voice'] = $voice_id;
              $tropo->say($text, $say_params);
            }
          }
          break;

        case 'VoipCmdSendText';
          $call->setCallStatus(VoipCall::IN_PROGRESS);
          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();

          $callback_url = $menu_path . '/process_sms_callback/' . $cid;
          $callback_continue = url($callback_url . '/continue', array('absolute' => TRUE));
          $callback_error = url($callback_url . '/error', array('absolute' => TRUE));
          $callback_hangup = url($callback_url . '/hangup', array('absolute' => TRUE)); 
          $callback_incomplete = url($callback_url . '/incomplete', array('absolute' => TRUE));
          
          $to = $script->evalString($cmd->getParam('to'));
          //If "to" is empty then send message to the caller
          if (!$to) {
            $to = $script->evalString($script->getVar('caller_number'));
          }
          $text = $script->evalString($cmd->getParam('text'));
          $network = $script->evalString($cmd->getParam('network'));
          if (voipscript_use_sms_framework() && (strtolower($network) == 'sms' || strtolower($call->getCurrentNetwork())=='sms')) {
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
            $params = array(
              'to' => $to,
              'channel' => 'TEXT',
              'network' => $network,
              );
            if (strtolower($network) == 'sms') {
              //We need caller id only for sms
              // Note: If no caller number provided, use the system's number as the default caller id
              $default_cid_number = variable_get('voipcall_cid_number', NULL);
              $caller_id = $script->evalString($cmd->getParam('caller_id', $default_cid_number)); 
              //$params['from'] = $caller_id;
            }
            $tropo->message($text, $params);
            //Register events:
            //$tropo->on(array("event" => "continue", "next" =>  $callback_continue));
            $tropo->on(array("event" => "error", "next" =>  $callback_error));
            //$tropo->on(array("event" => "hangup", "next" =>  $callback_hangup));
            $tropo->on(array("event" => "incomplete", "next" =>  $callback_incomplete));
          }
          $rc = TRUE;
          $processing = TRUE;
          break;

        case 'VoipCmdBeep';
          $beep_url = url('voip/sound/beep', array('absolute' => TRUE));
          $tropo->say($beep_url);
          break;

        case 'VoipCmdSet';
          $var_name = $script->evalString($cmd->getParam('var_name'));
          $var_value = $script->evalString($cmd->getParam('var_value'));
          $script->setVar($var_name, $var_value);
          break;

        case 'VoipCmdSetVoice';
          $voice = $script->evalString($cmd->getParam('voice'));
          //$voice_id = VoipVoice::getVoice($voice);
          $voice_id = VoipTropoServer::getVoiceId($voice,$language);
          $script->setVoice($voice_id);
          break;

        case 'VoipCmdUnset';
          $var_name = $script->evalString($cmd->getParam('var_name'));
          $script->unsetVar($var_name);
          break;
          
        case 'VoipCmdWait';
          $time_limit = $script->evalString($cmd->getParam('time_limit')) / 0.55;  //Looks like one period matches 0.55sec.
          $menu_path = 'voip/tropo/callhandler';
          $cid = $call->getCid();
          $response_url = $menu_path . '/process_wait/' . $cid;
          $redirect_url = url($response_url, array('absolute' => TRUE));
          //#1430227 - Equivalent of the wait function?: https://www.tropo.com/account/tickets/tickets.jsp?bb-cid=155&bb-tid=1429227&bb-name=tropo_support
          for ($i=0; $i<$time_limit; $i++) {
            $periods.=". ";
          }
          $tropo->say($periods);
          $tropo->on(array("event" => "continue", "next" =>  $redirect_url));
          $processing = FALSE;
          break;  
        
      }
      // go to next command
    }
  }

  // save script for next iteration
  $call->setScript($script);
  $call->save();
  return $rc;
}

function _voiptropo_file_save_upload($source, $validators, $dest) {
  global $user;
  $extensions='mp3 wav';
  // Add in our check of the the file name length.
  $validators['file_validate_name_length'] = array();

  // Begin building file object.
  $file = new stdClass();
  $file->filename = file_munge_filename(trim(basename($_FILES['filename']['name']), '.'), $extensions);
  $file->filepath = $_FILES['filename']['tmp_name'];
  $file->filemime = file_get_mimetype($file->filename);

  $file->source = $source;
  $file->destination = file_destination(file_create_path($dest . '/' . $file->filename), $replace);
  $file->filesize = $_FILES['filename']['size'];

  // Call the validation functions.
  $errors = array();
  foreach ($validators as $function => $args) {
    array_unshift($args, $file);
    // Make sure $file is passed around by reference.
    $args[0] = &$file;
    $errors = array_merge($errors, call_user_func_array($function, $args));
  }

  // Rename potentially executable files, to help prevent exploits.
  if (preg_match('/\.(php|pl|py|cgi|asp|js)$/i', $file->filename) && (substr($file->filename, -4) != '.txt')) {
    $file->filemime = 'text/plain';
    $file->filepath .= '.txt';
    $file->filename .= '.txt';
    // As the file may be named example.php.txt, we need to munge again to
    // convert to example.php_.txt, then create the correct destination.
    $file->filename = file_munge_filename($file->filename, $extensions);
    $file->destination = file_destination(file_create_path($dest . '/' . $file->filename), $replace);
  }


  // Check for validation errors.
  if (!empty($errors)) {
    $message = t('The selected file %name could not be uploaded.', array('%name' => $file->filename));
    if (count($errors) > 1) {
      $message .= '<ul><li>' . implode('</li><li>', $errors) . '</li></ul>';
    }
    else {
      $message .= ' ' . array_pop($errors);
    }
    drupal_set_message($message);
    return 0;
  }

  // Move uploaded files from PHP's upload_tmp_dir to Drupal's temporary directory.
  // This overcomes open_basedir restrictions for future file operations.
  $file->filepath = $file->destination;
  if (!move_uploaded_file($_FILES['filename']['tmp_name'], $file->filepath)) {
    form_set_error($source, t('File upload error. Could not move uploaded file.'));
    watchdog('file', 'Upload error. Could not move uploaded file %file to destination %destination.', array('%file' => $file->filename, '%destination' => $file->filepath));
    return 0;
  }

  // If we made it this far it's safe to record this file in the database.
  $file->uid = $user->uid;
  $file->status = FILE_STATUS_TEMPORARY;
  $file->timestamp = time();
  drupal_write_record('files', $file);
  return $file;
}

/**
 * Update 'call status' and 'hangup reason' based on tropo's response
 */
function voiptropo_update_call_status($call, $tropo_call_status, $request=NULL) {
 
  switch ($tropo_call_status) {
    default:
      $msg = 'Invalid CallStatus: @status for call cid: @cid';
      $cid = $call->getCid();
      $params = array('@status' => $call_status, '@cid' => $cid);
      $call->report_error('voiptropo', $msg, $params);
      return NULL;
      break;
    case 'incomplete': // call failed, most likely due to invalid number
      $call->setCallStatus(VoipCall::INVALID);
      $call->setHangupReason(VoipCall::HANGUP_INVALID_NUMBER);
      break;
    case 'ringing':
      $call->setCallStatus(VoipCall::RINGING);
      break;
    case 'in-progress':
      $call->setCallStatus(VoipCall::IN_PROGRESS);
      break;
    case 'continue':
      $call->setCallStatus(VoipCall::COMPLETED);
      $hangup_reason = $call->getHangupReason();
      if (!$hangup_reason) {
        $call->setHangupReason(VoipCall::HANGUP_BY_USER);
      }
      break;
    case 'busy':
      $call->setCallStatus(VoipCall::BUSY);
      $call->setHangupReason(VoipCall::HANGUP_BUSY);
      break;
    case 'error':
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
function _voiptropo_process_dial_response($call, $request, $dial_response_name) {
  $script = $call->getScript();
  // update the 'dial_sid' script variable
  $script->setVar('dial_sid', $request->getSessionId());//TODO: What is this? I used session id

  // update the 'dial_status' script variable
  $dial_status = $request->getState();
  switch (strtolower($dial_status)) {
    default:
      $cid = $call->getCid();
      $msg = 'Invalid DialCallStatus: @status for call cid: @cid';
      $params = array('@status' => $dial_status, '@cid' => $cid);
      $call->report_error('voiptropo', $msg, $params);
      return NULL;
      break;
    case 'completed':
    case 'answered':
      $script->setVar('dial_status', VoipScript::DIAL_COMPLETED);
      break;
    case 'busy':
      $script->setVar('dial_status', VoipScript::DIAL_BUSY);
      break;
    case 'no-answer':
      $script->setVar('dial_status', VoipScript::DIAL_NO_ANSWER);
      break;
    case 'disconnected':
      $script->setVar('dial_status', VoipScript::DIAL_FAILED);
      break;
    case 'canceled':
      $script->setVar('dial_status', VoipScript::DIAL_CANCELED);
      break;
  }

  $call->setScript($script);
  return $call;
}

/**
 * Process the response to a 'sms send' script command
 */
function _voiptropo_process_sms_response($call, $sms_status_tmp) {
  $cid = $call->getCid();

  switch ($sms_status_tmp) {
    default:
      $msg = 'Invalid SmsStatus: @status for call cid: @cid';
      $params = array('@status' => $sms_status_tmp, '@cid' => $cid);
      $call->report_error('voiptropo', $msg, $params);
      return NULL;
      break;
    case 'error':
      $msg = 'SMS sendind failed for call cid: @cid. Make sure your tropo number is sms-enabled.';
      $params = array('@cid' => $cid);
      watchdog('voiptropo', $msg, $params, WATCHDOG_WARNING);
      $sms_status = VoipCall::TEXT_FAILED;
      break;
    case 'incomplete':
      $msg = 'Invalid SMS parameter for call cid: @cid';
      $params = array('@cid' => $cid);
      watchdog('voiptropo', $msg, $params, WATCHDOG_WARNING);
      $sms_status = VoipCall::TEXT_INVALID;
      break;
    case 'sending':
      $sms_status = VoipCall::TEXT_SENDING;
      break;
    case 'continue':
      $sms_status = VoipCall::TEXT_SENT;
      break;
  }
  $call->setTextStatus($sms_status);
  return $call;
}

function _voiptropo_prompt_validate($script, $prompt) {
  if (is_string($prompt) && (substr($prompt, 0, 1) == '^')) {
    $prompt = $script->evalString($prompt);            
  }

  if (is_string($prompt)) {
    $prompt_list[] = new VoipPrompt($prompt);
  }
  elseif (is_object($prompt)) {
    $prompt_list[] = $prompt;
  }
  else {
    $prompt_list=$prompt;
  }
  return $prompt_list;
}

function _voiptropo_get_input($type, $call, $cmd,  $script, &$tropo){
  $call->setCallStatus(VoipCall::IN_PROGRESS);
  $menu_path = 'voip/tropo/callhandler';
  $cid = $call->getCid();

  $timeout = $script->evalString($cmd->getParam('timeout', 5.0)); 
  $end_key = $script->evalString($cmd->getParam('end_key', '')); 
  $default_language = $script->getLanguage();
  $default_voice = $script->getVoice();
          
  if($type=='voice'){
    $response_url = $menu_path . '/process_get_voice_input/' . $cid;
    $choices = $script->evalString($cmd->getParam('choices'));
    $mode = $script->evalString($cmd->getParam('mode'));

    $recognize_as_tmp = $script->evalString($cmd->getParam('recognize_as'));
    $default_region = VoipTropoServer::getVoiceRegionId($default_voice, $default_language);
    $recognizer = $recognize_as_tmp ? $recognize_as_tmp : $default_region;
            
    $allowed_modes = array('keypad', 'speech', 'any');
    if(in_array($mode, $allowed_modes)) {
      //Convert keypad to dtmf to match Tropo allowed mode values
      if($mode == 'keypad'){
          $mode = 'dtmf';
      }
    }
    else {
        $mode='speech';
    }
  }
  else {
    $response_url = $menu_path . '/process_get_input/' . $cid;
    $mode='dtmf';
    $num_digits = $script->evalString($cmd->getParam('num_digits'), 1);
    if (empty($num_digits)) {
      $num_digits = 1;
    }
    $grxml_path="voip/tropo/grammar/numeric_star_hash/". $num_digits;
    $choices = url($grxml_path, array('absolute' => TRUE));
  }
  $action = url($response_url, array('absolute' => TRUE));
  $attempts = 1;
  $timeout = (float)$timeout;
  $prompt = _voiptropo_prompt_validate($script, $cmd->getParam('prompt'));
  $ask_text = '<speak>';
    foreach ($prompt as $p) {
    $text = $script->evalString($p->getText());
    $text = _voiptropo_strip_special_chars($text);
    $voice_id = $p->getVoice() ? $p->getVoice() : $default_voice;
    $as = $p->getAs() ? $script->evalString($p->getAs()) : '';
    if (empty($as) && !in_array($as, array('digits', 'date'))) {
            $as = 'number'; 
    }
   
    
    //Tropo doesn't support multiple voices in ask(): https://www.tropo.com/bizblog/viewer?&bb-name=tropo_support&bb-q=&&bb-cid=99&bb-tid=1427328#bb
    //Therefore using SSML directly
    $text =_voiptropo_parse_audio($text);
    $ask_text .= '<voice name=\''.$voice_id.'\'><say-as interpret-as=\'vxml:'.$as.'\'>'.$text.'</say-as></voice>';
  }
  $ask_text .= '</speak>';
  $params = array(
    'choices' => $choices,
    'mode' => $mode,
    'terminator' => $end_key,
    'attempts' => $attempts,
    'timeout' => $timeout,
    'recognizer' => $recognizer,
  );
  $tropo->ask($ask_text, $params);
  $tropo->on(array("event" => "continue", "next" => $action)); 
}

function _voiptropo_parse_numbers($str) {
  $str_array = explode(" ",$str);
  foreach ($str_array as $key => $word) {
    if(!_voiptropo_is_url($word)){
      $str_array[$key] = preg_replace_callback('{(\d+)}', '_voiptropo_ssml_digits', $word);
    }
  }
  
  return implode(" ", $str_array);
}

/*Replace url with <audio> tag*/
function _voiptropo_parse_audio($str) {
  $str_array = explode(" ",$str);
  foreach ($str_array as $key => $word) {
    if(_voiptropo_is_url($word)){
      $str_array[$key] = "<audio src='".$word."'/>";
    }
  }
  
  return implode(" ", $str_array);
}

function _voiptropo_ssml_digits($matches) {
  return "<say-as interpret-as='vxml:digits'>".$matches[0]."</say-as>";
}

/**
 * Check if the given string is a url
 */
function _voiptropo_is_url($str) {
  // Note: Although the checking performed is not as complete as
  //       http://stackoverflow.com/questions/161738/what-is-the-best-regular-expression-to-check-if-a-string-is-a-valid-url , it seems to serve our purposes!
  $rc = preg_match('#(https?://\S+(?<![,.;?\:\!]))#', $str);
  return $rc;
}

function _voiptropo_strip_special_chars($string) {
  //Tropo doesn't support double quotes https://www.tropo.com/bizblog/viewer?&bb-name=tropo_support&bb-q=quotes&&bb-cid=100&bb-tid=1337679#bb
  $string = str_replace('"','&quot;',$string);
  //This would be better but it breaks SSML:
  //$string = htmlspecialchars($string);
  return $string;
}
