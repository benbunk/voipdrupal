<?php

// $Id$

/**
 * @file
 * Implementation of core functionality associated with voip calls
 */


/*
 * Class definitions
 */


/*
 * Class that keeps track of both the commands as well as the current status of
 * a voip call
 */
class VoipCall {

  /**
   * Instance variables
   */

  /**
   * VoipCall data
   */
  private $call_data;


  /**
   * Constants
   */

  /**
   * 'call status' field values
   */
  const ERROR = 'processing_error';

  const NOT_CALLED = 'not_called'; // the call has yet to be called
  const QUEUED = 'queued'; // the call is waiting in line before going out

  const RINGING = 'ringing'; // the call is currently ringing
  const IN_PROGRESS = 'in_progress'; // the call is in progress

  const COMPLETED = 'completed';
  const NO_ANSWER = 'no_answer';
  const BUSY = 'busy';
  const INVALID = 'invalid_number';
  const CANCELED = 'canceled'; // canceled by the system while queued or ringing

  /**
   * 'direction' field values
   */
  const INBOUND = 'inbound';
  const OUTBOUND = 'outbound';
  const OUTBOUND_DIAL = 'outbound_dial';

  /**
   * 'answered by' field values
   */
  const NOT_CHECKED = 'not_checked';
  const ANSWERED_DRUPAL = 'answered_by_drupal';
  const ANSWERED_MACHINE = 'answered_by_machine';
  const ANSWERED_HUMAN = 'answered_by_human';

  /**
   * 'text status' field values
   */
  const TEXT_FAILED = 'text_failed';
  const TEXT_INVALID = 'text_invalid';
  const TEXT_NOT_SENT = 'text_not_sent';
  const TEXT_SENDING = 'text_sending';
  const TEXT_SENT = 'text_sent';

  /**
   * 'hangup reason' field values
   */
  const HANGUP_BY_USER = 'hangup_by_user'; // call hangup by phone user
  const HANGUP_API_REQUEST = 'hangup_api_request'; // requested via server API
  const HANGUP_BUSY = 'hangup_busy';
  const HANGUP_INVALID_NUMBER = 'hangup_invalid_number';
  const HANGUP_NO_ANSWER = 'hangup_no_answer';
  const HANGUP_REJECTED = 'hangup_rejected';
  const HANGUP_SCRIPT_COMMAND = 'hangup_script_command';
  const HANGUP_SCRIPT_END = 'hangup_script_end';
  const HANGUP_SYSTEM_ERROR = 'hangup_system_error';


  /**
   * VoipCall constructor
   */
  function __construct($call_data = NULL) {
    if (is_array($call_data)) {
      $call_data = (object) $call_data;
    }
    $this->call_data = $call_data;
  }


  /**
   * Static methods
   */

  /**
   * Load a call from the database
   *
   * @param $cid
   *   The id of the call row
   *
   * @return
   *   The VoipCall instance, or NULL in case of an invalid cid
   */

  /**
   *
   * @todo Drupal 7 Update
   *
   * replace db_fetch_object() with foreach ()
   *
   * $call_data = db_fetch_object(db_query("SELECT * FROM {voipcall} WHERE cid = :cid", array(':cid' => $cid)));
   *
   * Should be replaced by
   *
   * foreach ($result as $record) {
   *
   *
   **/

  public static function load($cid) {
    $call = NULL;
    $result = db_query("SELECT * FROM {voipcall} WHERE cid = :cid", array(':cid' => $cid));
    foreach($result as $call_data) {
      if ($call_data) {
        // unserialize the call script
        $script = unserialize($call_data->script);
        $call_data->script = $script;

        $call = new VoipCall($call_data);
        break;
      }
    }
    return $call;
  }

  /**
   * Return the voipcall cid associated with the given call_id.
   *
   * @param string $call_id, the unique identifier of a call
   *
   * @return $cid or NULL depending on the result of the query
   */

  /**
   * @todo Drupal 7 update
   *
   * Reformat
   * $cid = db_query("SELECT cid FROM {voipcall} WHERE call_id = :call_id", array(':call_id' => $call_id))->fetchField();
   *
   * to match drupal standard
   *
   */

  public static function getCidFromCallId($call_id) {
    // fetch the desired voipcall data
    $cid = db_query("SELECT cid FROM {voipcall} WHERE call_id = :call_id", array(':call_id' => $call_id))->fetchField();
    return $cid;
  }



  /*
   * Public methods
   */

  /**
   * Save the call data to the database
   *
   * @return
   *   The updated VoipCall instance, or NULL in case of an invalid cid
   */
  public function save() {

    $call_data = $this->_voipcall_save($this->call_data);
    // unserialize the call script
    $script = unserialize($call_data->script);
    $call_data->script = $script;

    // unserialize the text script
    /*$text_script = unserialize($call_data->text_script);
     $call_data->text_script = $text_script;*/

    $this->call_data = $call_data;
    return $this;
  }

  /**
   * Delete the call data from the database
   *
   * @return
   *   TRUE or FALSE result of operation
   */
  public function delete() {
    $cid = $this->call_data->cid;
    // @todo Drupal 7 update Please review the conversion of this statement to the D7 database API syntax.
    /* db_query("DELETE FROM {voipcall} WHERE cid=%d", $cid) */
    $res = db_delete('voipcall')
  ->condition('cid', $cid)
  ->execute();
    $this->call_data = NULL;
    return $res;
  }

  /**
   * Check if the call is in a hangup state
   */
  public function isHangup() {
    $rc = FALSE;
    $status = $this->call_data->status;
    switch ($status) {
      case VoipCall::BUSY:
      case VoipCall::CANCELED:
      case VoipCall::ERROR:
      case VoipCall::INVALID:
      case VoipCall::NO_ANSWER:
      case VoipCall::COMPLETED:
        $rc = TRUE;
        break;
    }
    return $rc;
  }


  /**
   * Report a voipcall-related processing error
   *
   * @param string $module with the name of the module reporting the error
   *
   * @param string $msg with the error message with placeholders for the params
   *
   * @param array $params with pairs string->string that will replaced in $msg
   *
   * @return void
   */
  public function report_error($module, $msg, $params = array()) {
    watchdog($module, $msg, $params, WATCHDOG_ERROR);
    $msg_text = "($module) " . strtr($msg, $params);
    voip_error($msg_text);
    $this->setErrorMessage($msg_text);
    $this->setCallStatus(VoipCall::ERROR);
    $this->setHangupReason(VoipCall::HANGUP_SYSTEM_ERROR);
    $this->save();
  }



  /**
   * Getters and setters
   */

  public function getCallData() {
    // NOTE: in order to protect the architecture of this class, please
    // only use this method to access generic data info such as cid, uid, etc.
    return $this->call_data;
  }

  public function getCid() {
    return $this->call_data->cid;
  }

  public function getUid() {
    return $this->call_data->uid;
  }

  public function setUid($uid) {
    $this->call_data->uid = $uid;
  }

  public function getCallId() {
    return $this->call_data->call_id;
  }

  public function setCallId($cid) {
    $this->call_data->call_id = $cid;
  }

  public function getDestNumber() {
    return isset($this->call_data->dest_number) ? $this->call_data->dest_number : "";
  }

  public function setDestNumber($number) {
    $this->call_data->dest_number = $number;
  }

  public function getDestName() {
    return $this->call_data->dest_name;
  }

  public function setDestName($name) {
    $this->call_data->dest_name = $name;
  }

  public function getCallerNumber() {
    return isset($this->call_data->caller_number) ? $this->call_data->caller_number : "";
  }

  public function setCallerNumber($number) {
    $this->call_data->caller_number = $number;
  }

  public function getCallerName() {
    return isset($this->call_data->caller_name) ? $this->call_data->caller_name : "";
  }

  public function setCallerName($name) {
    $this->call_data->caller_name = $name;
  }

  public function getCallStatus() {
    return $this->call_data->status;
  }

  public function setCallStatus($status) {
    $this->call_data->status = $status;
  }

  public function getCallStatusDescription() {
    $description = NULL;
    $status = $this->getCallStatus();
    if (!$status) {
      $description = t('Not set.');
    }
    else {
      switch ($status) {
        default:
          $module = "voipcall";
          $msg = t('Invalid status passed to getCallStatusDescription(): @status.');
          $params = array('@status' => $status);
          $reason = $this->report_error($module, $msg, $params);
          break;
        case VoipCall::ERROR:
          $description = t('System error.');
          break;
        case VoipCall::NOT_CALLED:
          $description = t('Not called.');
          break;
        case VoipCall::QUEUED:
          $description = t('Queued.');
          break;
        case VoipCall::RINGING:
          $description = t('Ringing.');
          break;
        case VoipCall::IN_PROGRESS:
          $description = t('In progress.');
          break;
        case VoipCall::COMPLETED:
          $description = t('Completed.');
          break;
        case VoipCall::NO_ANSWER:
          $description = t('No answer.');
          break;
        case VoipCall::BUSY:
          $description = t('Busy.');
          break;
        case VoipCall::INVALID:
          $description = t('Invalid number.');
          break;
        case VoipCall::CANCELED:
          $description = t('Canceled.');
          break;
      }
    }
    return $description;
  }

  public function getAnsweredBy() {
    return $this->call_data->answered_by;
  }

  public function setAnsweredBy($answered_by) {
    $this->call_data->answered_by = $answered_by;
  }

  public function getTextStatus() {
    return $this->call_data->text_status;
  }

  public function setTextStatus($status) {
    $this->call_data->text_status = $status;
  }

  public function getSource() {
    return $this->call_data->source;
  }

  public function setSource($source) {
    $this->call_data->source = $source;
  }

  public function getDirection() {
    return $this->call_data->direction;
  }

  public function setDirection($direction) {
    $this->call_data->direction = $direction;
  }

  public function getStartTime() {
    return $this->call_data->start_time;
  }

  public function setStartTime($time = NULL) {
    if (!$time) {
      $time = REQUEST_TIME;
    }
    return $this->call_data->start_time = $time;
  }

  public function getEndTime() {
    return $this->call_data->end_time;
  }

  public function setEndTime($time = NULL) {
    if (!$time) {
      $time = REQUEST_TIME;
    }
    return $this->call_data->end_time = $time;
  }

  public function getDuration() {
    return $this->call_data->duration;
  }

  public function setDuration($time) {
    return $this->call_data->duration = $time;
  }

  public function getMaxSessionDuration() {
    return $this->call_data->max_session_duration;
  }

  public function setMaxSessionDuration($time_in_seconds) {
    return $this->call_data->max_session_duration = $time_in_seconds;
  }

  public function getErrorMessage() {
    return $this->call_data->error_message;
  }

  public function setErrorMessage($msg) {
    $this->call_data->error_message = $msg;
  }


  public function getScript() {
    $script = $this->call_data->script;
    $updated_script = $this->_voipcall_update_script($script);
    return $updated_script;
  }

  public function setScript($script) {
    if (!($script instanceof VoipScript)) {
      $this->report_error('VoipCall', 'Scripts must be an object of type VoipScript when calling VoipCall::setScript()  Input: @script', array('@script' => $script));
    }
    $this->call_data->script = $script;
  }

  public function getHangupScriptName() {
    return $this->call_data->hangup_script;
  }

  public function setHangupScriptName($name) {
    $this->call_data->hangup_script = $name;
  }

  public function getHangupReason() {
    return $this->call_data->hangup_reason;
  }

  public function setHangupReason($reason) {
    $this->call_data->hangup_reason = $reason;
  }

  public function getCurrentChannel() {
    return $this->call_data->current_channel;
  }

  public function setCurrentChannel($channel) {
    $this->call_data->current_channel = $channel;
    if (empty($this->call_data->call_channel)) {
      //Set initial channel
      $this->call_data->call_channel = $channel;
    }
  }

  public function getCallChannel() {
    return $this->call_data->call_channel;
  }

  public function setCallChannel($channel) {
    $this->call_data->call_channel = $channel;
  }

  public function getCurrentNetwork() {
    return $this->call_data->current_network;
  }

  public function setCurrentNetwork($network) {
    $this->call_data->current_network = $network;
    if (empty($this->call_data->call_network)) {
      //Set initial network
      $this->call_data->call_network = $network;
    }
  }

  public function getCallNetwork() {
    return $this->call_data->call_network;
  }

  public function setCallNetwork($network) {
    $this->call_data->call_network = $network;
  }

  public function getHangupReasonDescription() {
    $description = NULL;
    $reason = $this->getHangupReason();
    if (!$reason) {
      $description = t('Not hangup.');
    }
    else {
      switch ($reason) {
        default:
          $module = "voipcall";
          $msg = t('Invalid reason passed to getHangupSystemDescription(): @reason.');
          $params = array('@reason' => $reason);
          $reason = $this->report_error($module, $msg, $params);
          break;
        case VoipCall::HANGUP_BY_USER:
          $description = t('Call hangup by either caller or callee.');
          break;
        case VoipCall::HANGUP_API_REQUEST:
          $description = t('Call completed due to server request.');
          break;
        case VoipCall::HANGUP_BUSY:
          $description = t('Busy line.');
          break;
        case VoipCall::HANGUP_INVALID_NUMBER:
          $description = t('Invalid number.');
          break;
        case VoipCall::HANGUP_NO_ANSWER:
          $description = t('No answer.');
          break;
        case VoipCall::HANGUP_REJECTED:
          $description = t('Call rejected.');
          break;
        case VoipCall::HANGUP_SCRIPT_COMMAND:
          $description = t('Call completed due to dialplan script request.');
          break;
        case VoipCall::HANGUP_SCRIPT_END:
          $description = t('Reached end of dialplan script.');
          break;
        case VoipCall::HANGUP_SYSTEM_ERROR:
          $description = t('System error.');
          break;
      }
    }
    return $description;
  }

  public function getCheckMachine() {
    return $this->call_data->check_machine;
  }

  public function setCheckMachine($bool) {
    $this->call_data->check_machine = $bool;
  }

  public function setTextVariables($inbound_text_contents, $inbound_text_origin, $inbound_text_destination, $inbound_text_network) {
    $script = $this->call_data->script;
    $script->setVar('inbound_text_contents', $inbound_text_contents);
    $script->setVar('inbound_text_time_received', REQUEST_TIME);
    $script->setVar('inbound_text_origin', $inbound_text_origin);
    $script->setVar('inbound_text_destination', $inbound_text_destination);
    $script->setVar('inbound_text_network', $inbound_text_network);
    $this->call_data->script = $script;
  }


  /*
   * Private methods
   */

  /**
   * Save VoipCall data.
   *
   * @param
   *  $details, VoipCall instance with the fields to be saved
   *
   * @return
   *  $data, the newly saved call_data data.
   */
  private function _voipcall_save($details = NULL) {
    if (is_array($details)) {
      $details = (object) $details;
    }

    // if this is a new call data being created
    if (!isset($details->cid) || is_null($details->cid)) {
      // @TODO Why does this get called so many times when a call goes out?
      // @TODO Why doesn't this save new data every time a call blast goes out, e.g. unchecking blast sent ?

      $data = new stdClass();
      $update = NULL;
      $data->created = REQUEST_TIME;

      // set the data uid
      global $user;
      $data->uid = $user->uid;

      // initialize the 'Call Id' field
      if (!isset($details->call_id)) {
        $data->call_id = NULL;
      }

      // initialize 'Call Status' field
      if (!isset($details->status)) {
        $data->status = VoipCall::NOT_CALLED;
      }

      // initialize 'Answered By' field
      if (!isset($details->answered_by)) {
        $data->answered_by = VoipCall::NOT_CHECKED;
      }

      // initialize 'Text Status' field
      if (!isset($details->text_status)) {
        $data->text_status = VoipCall::TEXT_NOT_SENT;
      }

      // initialize 'Call Source' field
      if (!isset($details->source)) {
        $data->source = 'drupal';
      }

      // initialize 'Call Direction' field
      if (!isset($details->direction)) {
        $data->direction = VoipCall::OUTBOUND;
      }

      // initialize 'Channel' fields
      if (!isset($details->call_channel)) {
        $data->call_channel = 'voice';
      }
      if (!isset($details->current_channel)) {
        $data->current_channel = 'voice';
      }

      // initialize 'Network' fields
      if (!isset($details->call_network)) {
        $data->call_network = 'voip';
      }
      if (!isset($details->current_network)) {
        $data->current_network = 'voip';
      }

      // initialize voice call script with the default inbound/outbound one
      $dir_tmp = $details->direction;
      $direction = $dir_tmp ? $dir_tmp
                              : $data->direction;
      if (!isset($details->script)) {
        if ($details->current_channel == 'text' || $details->current_channel == 'offline_text') {
          $script_name = voipcall_get_default_inbound_text_script_name();
        }
        else {
          if ($direction == VoipCall::INBOUND) {
            $script_name = voipcall_get_default_inbound_call_script_name();
          }
          else {
            $script_name = voipcall_get_default_outbound_call_script_name();
          }
        }

        $default_script = VoipScript::loadScript($script_name);
        $data->script = serialize($default_script);
      }

      // initialize 'Hangup Reason' field
      if (!isset($details->hangup_reason)) {
        $data->hangup_reason = NULL;
      }

      // initialize 'Check Machine' field
      if (!isset($details->check_machine)) {
        $data->check_machine = FALSE;
      }

    }
    else {
      // load the data
      $result = db_query("SELECT * FROM {voipcall} WHERE cid = :cid", array(':cid' => $details->cid));
      $data = NULL;
      foreach($result as $info) {
        $data = $info;
        break;
      }
      $update = 'cid';
      $data->changed = REQUEST_TIME;
    }

    // update data fields
    if (isset($details->uid)) {
      $data->uid = $details->uid;
    }

    // update voipcall-specific fields

    $new_call_id = $details->call_id;
    if (isset($new_call_id)) {
      $data->call_id = $new_call_id;
    }

    $new_dest_number = isset($details->dest_number) ? $details->dest_number : NULL;
    if (isset($new_dest_number)) {
      $data->dest_number = $new_dest_number;
    }

    $new_dest_name = isset($details->dest_name) ? $details->dest_name : NULL;
    if (isset($new_dest_name)) {
      $data->dest_name = $new_dest_name;
    }

    $new_caller_number = isset($details->caller_number) ? $details->caller_number : NULL;
    if (isset($new_caller_number)) {
      $data->caller_number = $new_caller_number;
    }

    $new_caller_name = isset($details->caller_name) ? $details->caller_name : NULL;
    if (isset($new_caller_name)) {
      $data->caller_name = $new_caller_name;
    }

    $new_call_status = $details->status;
    if (isset($new_call_status)) {
      $data->status = $new_call_status;
    }

    $new_answered_by = $details->answered_by;
    if (isset($new_answered_by)) {
      $data->answered_by = $new_answered_by;
    }

    $new_text_status = isset($details->text_status) ? $details->text_status : NULL;
    if (isset($new_text_status)) {
      $data->text_status = $new_text_status;
    }

    $new_source = $details->source;
    if (isset($new_source)) {
      $data->source =  $new_source;
    }

    $new_direction = $details->direction;
    if (isset($new_direction)) {
      $data->direction =  $new_direction;
    }

    $new_start_time = $details->start_time;
    if (!isset($new_start_time)) {
      $new_start_time = REQUEST_TIME;
    }
    $data->start_time = $new_start_time;

    $new_end_time = isset($details->end_time) ? $details->end_time : NULL;
    if (isset($new_end_time)) {
      $data->end_time = $new_end_time;
    }

    $new_duration = isset($details->duration) ? $details->duration : NULL;
    if (isset($new_duration)) {
      $data->duration = $new_duration;
    }

    $new_max_session_duration = isset($details->max_session_duration) ? $details->max_session_duration : NULL;
    if (isset($new_max_session_duration)) {
      $data->max_session_duration = $new_max_session_duration;
    }

    $new_error_message = isset($details->error_message) ? $details->error_message : NULL;
    if (isset($new_error_message)) {
      $data->error_message = $new_error_message;
    }

    $call_channel = $details->call_channel;
    if (isset($call_channel)) {
      $data->call_channel = $call_channel;
    }

    $current_channel = $details->current_channel;
    if (isset($current_channel)) {
      $data->current_channel = $current_channel;
    }

    $call_network = $details->call_network;
    if (isset($call_network)) {
      $data->call_network = $call_network;
    }

    $current_network = $details->current_network;
    if (isset($current_network)) {
      $data->current_network = $current_network;
    }

    $new_script = isset($details->script) ? $details->script : NULL;
    if (isset($new_script)) {
      $data->script =  serialize($new_script);
    }

    $new_hangup_script_name = isset($details->hangup_script) ? $details->hangup_script : NULL;
    if (isset($new_hangup_script_name)) {
      $data->hangup_script =  $new_hangup_script_name;
    }

    $new_hangup_reason = isset($details->hangup_reason) ? $details->hangup_reason : NULL;
    if (isset($new_hangup_reason)) {
      $data->hangup_reason =  $new_hangup_reason;
    }

    $new_check = isset($details->check_machine) ? $details->check_machine : NULL;
    if (isset($new_check)) {
      $data->check_machine =  $new_check;
    }

    // set other optional data fields
    if (isset($details->other)) {
      foreach ($details->other as $field => $value) {
        $data->$field = $value;
      }
    }

    // save the data
    if(is_null($update)) {
      drupal_write_record('voipcall', $data);
    }
    else {
      drupal_write_record('voipcall', $data, $update);
    }

    return $data;
  }

  /**
   * Update script variables with the current status of the given call
   *
   * NOTE: The call variables are all read-only. Any changes to their values
   *       executed within the script will not be applied to the actual
   *       call data.
   *
   * @param VoipScript $script to be updated
   *
   * @return updated script.
   */
  private function _voipcall_update_script($script) {

    $call_data = $this->call_data;

    // add call variables to script variables

    $script->setVar('uid', $call_data->uid);

    $script->setVar('call_id',
      $call_data->call_id);

    $script->setVar('dest_number',
      $call_data->dest_number);
    $script->setVar('dest_name',
      $call_data->dest_name);
    $script->setVar('caller_number',
      $call_data->caller_number);
    $script->setVar('caller_name',
      $call_data->caller_name);

    $script->setVar('call_status',
      $call_data->status);

    $script->setVar('answered_by',
      $call_data->answered_by);

    $script->setVar('text_status',
      $call_data->text_status);

    $script->setVar('source',
      $call_data->source);
    $script->setVar('direction',
      $call_data->direction);

    $script->setVar('start_time',
      $call_data->start_time);
    $script->setVar('end_time',
      $call_data->end_time);
    $script->setVar('duration',
      $call_data->duration);

    $script->setVar('max_session_duration',
      $call_data->max_session_duration);

    $script->setVar('error_message',
      $call_data->error_message);

    $script->setVar('hangup_reason',
        $call_data->hangup_reason);

    $script->setVar('check_machine',
        $call_data->check_machine);

    return $script;
  }
}
