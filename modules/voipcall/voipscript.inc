<?php

// $Id$

/**
 * @file
 * Implementation of core functionality associated with voip scripts
 */


/*
 * Class definitions
 */

        
/*
 * Class that keeps track of both the commands as well as the current status of 
 * a VoIP Drupal Script
 */
class VoipScript {
  
  // Script variables

  /**
   * Script name
   */
  private $name;

  /**
   * Array of script commands
   */
  private $commands; 

  /**
   * Index of the next command to be executed
   */
  private $index; // index of the next command to be executed

  /**
   * Script variables
   */
  private $variables;

  /**
   * Stack of scripts being executed via gosub commands.
   *
   * Each element of the array stores the current status of a script,
   * including name, commands, and command index
   */
  private $stack;

  /**
   * The id of the voice being currently used for text-to-speech generation
   */
  private $voice_id;

  /**
   * Flag indicating script execution error
   */
  protected $is_error;

  /**
   * Script error message
   */
  protected $error_message;


  /**
   * Constants
   */

  /**
   * Error code
   */
//TODO: do we really need this constant?
  const ERROR='processing_error';

  /**
   * Values associated with the dial command results
   */
  const DIAL_COMPLETED='dial_completed';
  const DIAL_BUSY='dial_busy';
  const DIAL_NO_ANSWER='dial_no_answer';
  const DIAL_FAILED='dial_failed';
  const DIAL_CANCELED='dial_canceled';
 

  /**
   * 'Reject' command options
   */
  const REJECT_INVALID='reject_invalid';
  const REJECT_BUSY='reject_busy';

  /**
   * VoipScript constructor
   *
   * @param $name 
   *   A string containing the name of the new script
   * @param $variables 
   *   An array with the initial value of the script variables
   *
   * @return
   *  The new script created
   */
  function __construct($name, $variables=array()) {
    $this->name = $name;
    $this->commands = array();
    $this->index = 0;
    $this->variables = $variables;
    $this->stack = array();
    $this->is_error = FALSE;
    $this->error_message = NULL;
  }

   /**
   * Returns the name of all the scripts available in the system
   *
   * @return
   *   Array with the script names sorted alphabetically
   */
  public static function getScriptNames() {
    $script_names = array();
    // Invoke hook_voipscript_get_script_names()
    foreach (module_implements('voipscript_get_script_names') as $module) {
      $function = $module .'_'. 'voipscript_get_script_names';
      $new_names = call_user_func($function);
      if(isset($new_names)) {
        // include new script names
        $script_names = array_merge($script_names, $new_names);
      }
    }
    sort($script_names);
    foreach($script_names as $k => $v) $sorted_names[$v] = $v;
    return $sorted_names;
  }



  /**
   * Returns a new instance of an already-existing script
   *
   * @param $script_name
   *   String containing the name of the script to be loaded
   * @param array $params
   *   Array with the value pairs to be passed to hook_voipscript_load_script()
   *
   * @return
   *   The new script instance, or NULL in case of invalid script name
   */
  public static function loadScript($script_name, $params = NULL){
    $rc = NULL;
    $script_tmp = new VoipScript($script_name);
    $script_tmp->runLoad($script_name, $params);
    if(!$script_tmp->isError()) {
      $rc = $script_tmp;
    }
    return $rc;
  }
        

  /*
   * Public methods
   */

  /**
   * Returns the script name
   */
  function getName() {
    return $this->name;
  }

  /**
   * Returns the value of the specified script variable
   *
   * NOTE: This method is only to be used inside VoIP Drupal server modules.
   *       To retrieve the value of a variable from inside the script execution,
   *       use the %variable_name notation as in addSay("You typed: %input").
   *       In this case, %input is going to be replaced by the value of the
   *       script variable 'input'
   */
  function getVar($name) {
    return $this->variables[$name];
  }

  /**
   * Sets the value of the specified script variable
   *
   * NOTE: This method is only to be used inside VoIP Drupal server modules.
   *       VoIP Scripts should use the addSet() method
   */
  function setVar($name, $value) {
    $this->variables[$name] = $value;
  }

  /**
   * Unsets the specified script variable
   */
  function unsetVar($name) {
    unset($this->variables[$name]);
  }

  /**
   * Returns the current language of the script
   */
  function getLanguage() {
    $language = VoipVoice::getVoice($this->voice_id)->getLanguage();
    return $language;
  }

  /**
   * Sets the current voice of the script
   */
  function setVoice($voice_id) {
    $this->voice_id = $voice_id;
  }

  /**
   * Returns the current voice of the script
   */
  function getVoice() {
    return VoipVoice::getVoice($this->voice_id)->getVoiceId();
    /*$voice_id = $this->voice_id;
    if(!$voice_id) {
      $voice_id = VoipVoice::getDefaultVoice()->getVoiceId();
    }
    return $voice_id;*/
  }


  /*
   * Methods used during script execution
   */

  /**
   * Returns the next script command to be executed
   */
  function getNextCommand() {
    $cmd = $this->commands[$this->index];
    $this->index++;
    return $cmd;
  }

  /**
   * Resets the script index
   */
  function resetIndex() {
    $this->index = 0;
  }

  /**
   * Evaluates the given string based on the current value of script variables.
   *
   * @param $string
   *   The string (or array of strings) to be evaluated
   *
   * @return 
   *   The result of the evaluation
   *
   * @see _voipscript_expand_variables() for the actual syntax allowed in the 
   *   string to be evaluated
   */
  function evalString($string) {
    if(!is_array($string)) {
      $rc = _voipscript_expand_variables($string, $this->variables);
    }
    else {
      foreach($string as $index => $str) {
        $rc[$index] = _voipscript_expand_variables($str, $this->variables);
      }
    }
    return $rc;
  }

  /**
   * Returns TRUE in case a script processing error has been detected
   */
  function isError(){
    return $this->is_error;
  }

  /**
   * Retuns the message associated with a script error.
   */
  function getErrorMessage() {
    return $this->error_message;
  }

  /*
   * Supported script commands
   */

  /**
   * Adds a beep command to the script.
   */
  function addBeep() {
    $this->commands[] = new VoipCommand('VoipCmdBeep');
  }

  /**
   * Adds a "dial" command to the script.
   *
   * When executed, the 'dial' command connects the current caller to another
   * phone. If the called party picks up, the two parties are connected and
   * can communicate until one hangs up. If the called party does not pick up,
   * or if a busy signal is received, or the number doesn't exist, the dial
   * command will finish.
   *
   * @param $number
   *   String with a valid number to be dialed
   *
   * @param $timeout
   *   Integer with the number of seconds to wait for the called party to
   *   answer the call
   *
   * @param $hangup_on_star
   *   If TRUE, lets the calling party hang up on the called party by pressing
   *   the '*' key on his phone. This feature allows the calling party to hang
   *   up on the called party without having to hang up the phone and end the
   *   session. When the caller presses '*' Voip Drupal will hang up on the
   *   called party and will move on to execute the next command in the script.
   *
   * @param $time_limit
   *   Integer with the maximum duration of the call in seconds
   *
   * @param $caller_id
   *   String with the caller id to be used for the call. By default, this is
   *   the caller id defined in the 'default call configuration' settings
   *
   * @return
   *   The 'dial' command sets the script variable dial_status with the
   *   outcomes of the call:
   *   - VoipScript::DIAL_COMPLETED
   *   - VoipScript::DIAL_BUSY
   *   - VoipScript::DIAL_NO_ANSWER
   *   - VoipScript::DIAL_FAILED
   *   - VoipScript::DIAL_CANCELED
   *
   *   If the call was successful, the variable dial_duration gets set with the
   *   number of seconds that the call lasted for.
   */
  function addDial($number='', $timeout=30, $hangup_on_star=false, $time_limit=3600, $caller_id=''){
    $params['number'] = $number;
    $params['timeout'] = $timeout;
    $params['hangup_on_star'] = $hangup_on_star;
    $params['time_limit'] = $time_limit;
    $params['caller_id'] = $caller_id;
    $this->commands[] = new VoipCommand('VoipCmdDial', $params);
  }
  
  /**
   * Adds a 'get input' command to the script. 
   *
   * When executed, the 'get input' command reads user input from the phone
   * keypad. The system stops reading the input whenever there is a
   * timeout, the user presses a predefined key, or the users presses the
   * maximum number of digits expected.
   *
   * @param $prompt 
   *   Mixed. Can be any of the following
   *   - an executable string to be processed into a prompt string, a
   *     VoipPrompt instance, or an array of VoipPrompt instances
   *   - a prompt string combining text and audio URLs to be played to the user
   *     prompting him/her to type something
   *   - a VoipPrompt instance
   *   - an array of VoipPrompt instances to be processed
   *
   * @param $timeout
   *   Integer specifying the maximum number of seconds to wait in between
   *   digits
   *
   * @param $end_key
   *   Key used to end the data input. The allowed values are the numbers 0-9,
   *   # , * and the empty string.
   *
   * @param $num_digits
   *   Integer with the number of digits that are expected.
   *
   * @return
   *   The 'get input' command sets the script variable input_digits to the
   *   input received.  In case nothing is typed, input_digits gets set to ''
   */
  function addGetInput($prompt='', $num_digits=NULL, $end_key='#', $timeout=5) {
    $params['prompt'] = $prompt;
    $params['num_digits'] = $num_digits;
    $params['end_key'] = $end_key;
    $params['timeout'] = $timeout;
    $this->commands[] = new VoipCommand('VoipCmdGetInput', $params);
  }

  /**
   * Adds a "gosub" command to the script.
   *
   * When executed, the 'gosub' command saves the state of the current script
   * into the local stack and loads the commands and variables of specified
   * script.
   * 
   * @param $script_name
   *   The name of the script to be loaded
   *
   * @param $options
   *   An array with options to be passed to the function that generates the
   *   required script
   *
   * @see VoipScript::addReturn()
   */
  function addGosub($script_name, $options=array()) {
    $params['script_name'] = $script_name;
    $params['options'] = $options;
    $this->commands[] = new VoipCommand('VoipCmdGosub', $params);
  }

  /** 
   * Adds a "go to" command to the script.
   * 
   * When ran, the 'go to' command updates the index of the script and skips
   * the script execution to the command that follows the specified label.
   *
   * @param $label_name
   *   String with the name of the label to go to.
   *
   * @see VoipScript::addLabel()
   * @see VoipScript::addGotoIf()
   */ 
  function addGoto($label_name) {
    $params['label_name'] = $label_name;
    $this->commands[] = new VoipCommand('VoipCmdGoto', $params);
  }

  /**
   * Adds a 'goto if' command to the script.
   *
   * This command is similar to VoipScript:addGoto(). However, it only goes to
   * the specified label in case the specified condition evaluates to TRUE.
   *
   * @param $label_name
   *   String with the name of the label to go to.
   * @param $condition
   *   If TRUE, skips script execution to the specified label.
   *
   * @see VoipScript::addLabel()
   * @see VoipScript::addGoto()
   */
  function addGotoIf($label_name, $condition) {
    $params['label_name'] = $label_name;
    $params['condition'] = $condition;
    $this->commands[] = new VoipCommand('VoipCmdGotoIf', $params);
  }

  /**
   * Adds a 'hangup' command to the script
   *
   *  @param $hangup_mode
   *  String with one of the following options:
   *  - "end_session", to end the session and leave the script index as is
   *  - "reset", to keep the session alive, but resetting both the stack and 
   *    the command index to their initial values
   *  - "resume", to keep the session alive, maintaining both the stack and 
   *   the command index with their present values
   */
  function addHangup($hangup_mode='end_session') {
    $params['hangup_mode'] = $hangup_mode;
    $this->commands[] = new VoipCommand('VoipCmdHangup', $params);
  }

  /**
   * Adds a 'join conference' command to the script.
   *
   * When executed, the 'join conference' command connects the current call
   * to the specified conference room.
   *
   * @param $room 
   *   String with the name of the conference call room
   *
   * @param $muted
   *   Boolean indicating whether the participant can speak on the conference
   *
   * @param $hangup_on_star
   *   If TRUE, allows participants to leave the conference by pressing '*'
   *
   * @return
   *   The 'join conference' command sets the script variable dial_status with
   *   the outcomes of the conference call:
   *   - VoipScript::DIAL_COMPLETED
   *   - VoipScript::DIAL_FAILED
   *
   */
  function addJoinConference($room, $muted=FALSE, $hangup_on_star=true) {
    $params['room'] = $room;
    $params['muted'] = $muted;
    $params['hangup_on_star'] = $hangup_on_star;
    $this->commands[] = new VoipCommand('VoipCmdJoinConference', $params);
  }

  /**
   * Adds a label to the script.
   *
   * @see VoipScript::addGoto()
   * @see VoipScript::addGotoIf()
   */
  function addLabel($label_name) {
    $params['label_name'] = $label_name;
    $this->commands[] = new VoipCommand('VoipCmdLabel', $params);
  }

  /**
   * Adds a log command to the script.
   */
  function addLog($input, $name=NULL) {
    $params['input'] = $input;
    $params['name'] = $name;
    $this->commands[] = new VoipCommand('VoipCmdLog', $params);
  }

  /**
   * Adds a "run IVR menu" command to the script
   * 
   * When executed, the "run IVR menu" command presents the user with a set of 
   * options and expects for an input to be typed in.
   *
   * @param $prompt
   *   Mixed element presenting the different menu items available ('For x,
   *   press 1. For y, press 2...'). If empty (''), the default message will
   *   be used: 'Press any key from the telephone keypad.'
   *   Can be any of the following
   *   - an executable string to be processed into a prompt string, a
   *     VoipPrompt instance, or an array of VoipPrompt instances
   *   - a prompt string combining text and audio URLs to be played to the user
   *   - a VoipPrompt instance
   *   - an array of VoipPrompt instances to be processed
   *
   * @param $input_options
   *   Array whose associating keys pressed with a respective script label.
   *   The indexes of the array are the keys allowed for the IVR. The contents
   *   are the script labels associated with each of those keys.  For instance,
   *   $options[1] = 'story_playback' would associate the 1 with the
   *   'story_playback' label of the script.
   *   By default, $input_options is set to the following:
   *     $input_options = Array (
   *       0 => 'option_0', 
   *       1 => 'option_1', 
   *       2 => 'option_2', 
   *       3 => 'option_3', 
   *       4 => 'option_4', 
   *       5 => 'option_5', 
   *       '*' => 'option_star', 
   *       '#' => 'option_hash', 
   *       't' => 'no_input_received', // used to report timeout situations 
   *       'i' => 'invalid_option'
   *     ) 
   *   If the option 't' is not provided, it will be set to 'no_input_received'
   *   by default. Similarly, by default 'i' will be set to 'invalid_option'.
   *     
   * @param input $invalid_msg
   *   Mixed. The message to be played in case an invalid option is
   *   selected. If empty (''), the default message will be used: 'Invalid
   *   option selected.' Similar to the $prompt parameter, might be either a
   *   string, a VoipPrompt instance, or an array of VoipPrompt instances.
   * 
   * @param %no_input_msg
   *   Mixed. The message to be played in case no input is received. If
   *   empty, will use the same message as $invalid_msg. Similar to the $prompt
   *   parameter, might be either a string, a VoipPrompt instance, or an array
   *   of VoipPrompt instances.
   *
   * @param $max_attempts
   *   String with the maximum number of attempts allowed.  Default is 3.
   * 
   * @param $timeout
   *   String with the number of seconds to wait for an input digit to be
   *   pressed. The default is 5
   *
   * @return
   *   Set the %ivr_option_selected script variable with the label associated
   *   with the option selected.
   */
  function addRunIvrMenu($prompt='', $input_options=NULL, $invalid_msg='', $no_input_msg='', $max_attempts=3, $timeout=5) {
    $default_prompt = t('Press any key from the telephone keypad.');
    $prompt = $prompt ? $prompt : $default_prompt;
    $params['prompt'] = $prompt;
    $default_input_options = Array (
      0 => 'option_0', 
      1 => 'option_1', 
      2 => 'option_2', 
      3 => 'option_3', 
      4 => 'option_4', 
      5 => 'option_5', 
      '*' => 'option_star', 
      '#' => 'option_hash', 
      't' => 'no_input_received', // used to report timeout situations 
      'i' => 'invalid_option'
    );
    $input_options = $input_options ? $input_options : $default_input_options;
    $input_options['t'] = $input_options['t']
                        ? $input_options['t'] : 'no_input_received';
    $input_options['i'] = $input_options['i']
                        ? $input_options['i'] : 'invalid_option';
    $params['input_options'] = $input_options;
    $msg_tmp = t('Invalid option selected.');
    $invalid_msg = $invalid_msg ? $invalid_msg : $msg_tmp;
    $params['invalid_msg'] = $invalid_msg;
    $no_input_msg = $no_input_msg ? $no_input_msg : $invalid_msg;
    $params['no_input_msg'] = $no_input_msg;
    $params['max_attempts'] = $max_attempts;
    $params['timeout'] = $timeout;
    $this->commands[] = new VoipCommand('VoipCmdRunIvrMenu', $params);
  }

  /**
   * Adds a 'record' command to the script.
   *
   * When executed, the 'record' command records audio input from the phone.
   * The system stops recording whenever there is a timeout, the user presses
   * the predefined 'end' key, or the maximum length of the recording is
   * reached.
   *
   * @param $prompt 
   *   Mixed. Can be any of the following
   *   - an executable string to be processed into a prompt string, a
   *     VoipPrompt instance, or an array of VoipPrompt instances
   *   - a prompt string combining text and audio URLs to be played to the user
   *     prompting him/her to start recording
   *   - a VoipPrompt instance
   *   - an array of VoipPrompt instances to be processed
   *
   * @param $timeout
   *   Integer specifying the maximum number of seconds of silence to wait
   *   before ending the recording.
   *
   * @param $end_key
   *   Key used to end the recording. The allowed values are the numbers 0-9,
   *   # , * and the empty string.
   *
   * @param $max_length
   *   Integer with the maximum number of seconds allowed for the recording
   *
   * @param $format
   *   The format in which to record. By default, servers should support at
   *   least the 'mp3' format
   *
   * @param $play_beep
   *   Boolean indicating whether or not to play a beep before the recording begins
   *   It's set to TRUE by default.
   *
   * @return
   *   The execution of the 'record' command sets the following script
   *   variables: 
   *   - recording_path, with the local location of the file recorded
   *   - recording_public_url, with the remote location of the file recorded if
   *     available, otherwise will store local location.
   *   - recording_duration, with the duration in seconds of the recording
   *   - recording_digits, the key (if any) pressed to end the recording
   *   - recording_fid, with the fid of the file recorded
   *
   *   NOTE: If the call is hang up in the middle of a recording, the variable 
   *   "recording_digits" will be set to "hangup" and the other variables will 
   *   be set to their appropriate values.
   */
  function addRecord($prompt='', $timeout=5, $end_key='#', $max_length=3600, $format='mp3', $play_beep=TRUE) {
    $params['prompt'] = $prompt;
    $params['timeout'] = $timeout;
    $params['end_key'] = $end_key;
    $params['max_length'] = $max_length;
    $params['format'] = $format;
    $params['play_beep'] = $play_beep;
    $this->commands[] = new VoipCommand('VoipCmdRecord', $params);
  }

  /**
   * Adds a 'reject' command to the script.
   *
   * Rejects an incoming call without billing you. Has to be the first command
   * to be executed for an incoming call.
   */
  function addReject($reason=VoipScript::REJECT_INVALID) {
    $params['reason'] = $reason;
    $this->commands[] = new VoipCommand('VoipCmdReject', $params);
  }

  /**
   * Add a 'return' command to the script.
   *
   * Returns call control to the previous script in the script stack.
   */
  function addReturn() {
    $this->commands[] = new VoipCommand('VoipCmdReturn');
  }

  /**
   * Add a 'say' command to the script.
   *
   * Says the given prompt the specified number of times.
   *
   * @param $prompt
   * String combining text to be converted to speech and audio URLs to be
   * played.
   * Mixed. Can be any of the following:
   * - a prompt string combining text to be converted to speech and audio
   * URLs to be played to the user
   * - a VoipPrompt instance
   * - an array of VoipPrompt instances to be processed
   * - an executable string that, when processed returns any of the above
   * options
   *
   * @param $loop
   *   Integer with the number of times that the prompt should be repeated
   *
   * @return
   *   The execution of the 'say' command does not return anything.
   */
  function addSay($prompt, $loop=1) {
    $params['prompt'] = $prompt;
    $params['loop'] = $loop;
    $this->commands[] = new VoipCommand('VoipCmdSay', $params);
  }

  /** 
   * Adds a "send text" command to the script.
   * 
   * When ran, this command sends the given text message to the specified phone
   * number. 
   * 
   * @uses VoipScript::$variables
   *
   * @param $text
   *   String with the text to be sent
   *
   * @param $to
   *   The destination of the text message. For instance, for messages sent via
   *   the SMS channel, $to contains the SMS number; for AIM messages, it
   *   contains the AIM username. When sending text as part of an inbound call,
   *   'to' defaults to the caller.  When sending text on an outbound call, 'to'
   *   defaults to the called party.
   *
   * @param $network
   *   String with the network to be used. The default the network is 'sms'. 
   *   Different VoIP servers might support other options such as 'aim', 'msn',
   *   'jabber', 'yahoo', 'gtalk', etc.
   *
   * @param $caller_id
   *   String with the caller id to be used. Depending on the network, it can be
   *   a phone number, a IM username, etc. When sending on an inbound call,
   *   'caller_id' defaults to the called party.  When sending on an outbound
   *   call, 'caller id' defaults to the calling party.
   * 
   * @return
   *   The execution of the 'send text' command sets the script variable
   *   'send_text_status' 
   *   to any of the following values:
   *   - VoipCall::TEXT_FAILED
   *   - VoipCall::TEXT_INVALID
   *   - VoipCall::TEXT_SENDING
   *   - VoipCall::TEXT_SENT
   */ 
  function addSendText($text, $to='', $network='SMS', $caller_id=''){
    $params['to'] = $to;
    $params['text'] = $text;
    $params['network'] = $network;
    $params['caller_id'] = $caller_id;
    $this->commands[] = new VoipCommand('VoipCmdSendText', $params);
  }

  /**
   * Set the value of the specified script variable
   */
  function addSet($var_name, $var_value) {
    $params['var_name'] = $var_name;
    $params['var_value'] = $var_value;
    $this->commands[] = new VoipCommand('VoipCmdSet', $params);
  }

  /**
   * Set the current language of the script
   */
  /*function addSetLanguage($language='en') {
    $params['language'] = $language;
    $this->commands[] = new VoipCommand('VoipCmdSetLanguage', $params);
  }
*/
  /**
   * Set the current voice of the script
   */
  function addSetVoice($voice='man') {
    $params['voice'] = $voice;
    $this->commands[] = new VoipCommand('VoipCmdSetVoice', $params);
  }

  /**
   * Unset the specified script variable
   */
  function addUnset($var_name) {
    $params['var_name'] = $var_name;
    $this->commands[] = new VoipCommand('VoipCmdUnset', $params);
  }
  
  /**
   * Adds a 'wait' command to the script.
   *
   * @param $time_limit
   * Number of seconds the script should wait before moving to the next command
   */
  function addWait($time_limit=60) {
    $params['time_limit'] = $time_limit;
    $this->commands[] = new VoipCommand('VoipCmdWait', $params);
  }


  /*
   * Execution of basic script commands that are not dependent on the voip
   * server
   */

  /**
   * Executes the specified script and then returns to the current script
   */
  function runGosub($script_name, $options=NULL) {
    $script_state = array('name' => $this->name,
                          'commands' => $this->commands,
                          'index' => $this->index);
    $this->stack[] = $script_state;
    $rc =  $this->runLoad($script_name, $options);
    return $rc;
  }

  /**
   * Jumps script execution to the command associated with the specified label
   */
  function runGoto($label_name) {
    $rc = TRUE;
    $found = FALSE;
    $i = 0;
    while(!$found) {
      $cmd = $this->commands[$i];
      if(!$cmd) {
        // label nor found
        $this->is_error = TRUE;
        $script_name = $this->name;
        $this->error_message = "Label '$label_name' not found in script '$script_name': " . print_r($this, true);
        $rc = FALSE;
        $found = TRUE;
      }
      else {
        if($cmd->getId() == 'VoipCmdLabel') {
          $string_tmp = $cmd->getParam('label_name');
          $label_name_tmp = $this->evalString($string_tmp);
          if(strcasecmp($label_name_tmp, $label_name) == 0) {
            $this->index = $i;
            $found = TRUE;
          }
        }
        // move to the next command in the script
        $i++;
      }
    }
    return $rc;
  }

  /**
   * Runs the specified IVR menu
   */
  function runIvrMenu($options) {
    // execute the predefined IVR script menu
    $script_name = 'voipscript_run_ivr_menu';
    $this->runGosub($script_name, $options);
  }

  /**
   * Loads the specified script
   *
   * @param $script_name
   *  string, name of script to be loaded
   * 
   * @param $params
   *  array, optional parameters 
   *  to be passed to implementations of hook_voipscript_load_script
   */
  function runLoad($script_name, $params = NULL) {
    $rc = TRUE;
    $found = FALSE;

    // Invoke hook_voipscript_load_script() giving modules the 
    // opportunity to return a custom script. 
    foreach (module_implements('voipscript_load_script') as $module) {
      $function = $module .'_'. 'voipscript_load_script';
      $new_script = call_user_func($function, $script_name, $params);
      if(isset($new_script)) {
        $found = TRUE;
        // loads new name and commands
        $this->name = $new_script->getName();
        $this->commands = $new_script->getCommands();
        $this->index = 0;
        // override variables with the same name and preserve the others
        $this->variables = array_merge($this->variables, $new_script->getVariables());
        // preserve the stack as is
          // $this->stack = array();
        $this->is_error = FALSE;
        // scripts must have unique names. exit the foreach
        break;
      }
    }

    if(!$found) {
      // no script found with the given name
      $rc = FALSE;
      $this->is_error = TRUE;
      $this->error_message = "Script '$script_name' not found for script '" . $this->name . "': " . print_r($this->commands, TRUE);
    }

    return $rc;
  } 

  /**
   * Logs the given message
   */
  function runLog($input, $name=NULL) {
    $options = array('@name' => $name, '@input' => $input);
    if($name) {
      $msg = "(log) @name :: @input";
    }
    else {
      $msg = "(log) @input";
    }
    watchdog('voipscript', $msg, $options);
  }

  /**
   * Returns to the script that has originally issued the Gosub that started the
   * present script
   */
  function runReturn() {
    $rc = TRUE;
    $script_state = array_pop($this->stack);
    if ($script_state) {
      $this->name = $script_state['name'];
      $this->commands = $script_state['commands'];
      // Note: index is already pointing to the next command to be executed
      $this->index = $script_state['index'];
    }
    else {
      // invalid return command
      $this->is_error = TRUE;
      $this->error_message = "Invalid return command for script '" . $this->name . "': " . print_r($this, true);
    }
    return $rc;
  }
  
  /**
   * Returns the number of scripts in the stack.
   */
  function getStackLevel() {
    return count($this->stack);
  }


  /**
   * Protected methods
   */

  protected function getCommands() {
    return $this->commands;
  }

  protected function getVariables() {
    return $this->variables;
  }

  // the following function is provided in case there is a need to
  // extend the script API to include additional commands
  protected function addCommand($command) {
    $this->commands[] = $command;
  }

}


/**
 * Implementation of command-related classes
 */

class VoipCommand {

  private $id; // command id
  private $params; // command parameters

  function __construct($id, $params=array()) {
    $this->id = $id;
    $this->params = $params;
  }

  function getId() {
    return $this->id;
  }

  function getParam($param_name, $default=NULL) {
    return $this->params[$param_name]?$this->params[$param_name]:$default;
  }
}
   

class VoipPrompt {

  /**
   * @variable $text 
   *   String combining text and audio URLs to be played to the user
   */
  private $text;
    
  /**
   * @variable $voice_id
   *   String with the id of voice to be used for the prompt.  If NULL, use
   *   the script's current voice
   */
  private $voice_id;

  /**
   * @variable $as
   *   String that determines how numbers should be pronounced. Supported
   *   values include 'number' (to read '12' as 'twelve') or 'digits' (to read
   *   '12' as 'one two'). By default, numbers should be read as 'number'
   */
  private $as;

  function __construct($text, $voice_id=NULL, $as=NULL) {
    $this->text = $text;
    $this->voice_id = $voice_id;
    $this->as = $as;
  }

  function getText() {
    return $this->text;
  }

  function getVoice() {
    return VoipVoice::getVoice($this->voice_id)->getVoiceId();
  }

  function getAs() {
    return $this->as;
  }

  // methods created to handle the fact that json_encode does not deal with
  // private properties

  private function to_array() {
    $a = array();
    $a['text'] = $this->text;
    $a['voice_id'] = $this->voice_id;
    $a['as'] = $this->as;
    return $a;
  }

  private static function from_array($a) {
    $prompt = new VoipPrompt($a['text'], $a['voice_id'], $a['as']);
    return $prompt;
  }

  static function encode($prompt) {
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
      $to_encode[] = $p->to_array();
    }
    $encoded = json_encode($to_encode);
    return $encoded;
  }

  static function decode($encoded) {
    $array_list = json_decode($encoded, true);
    foreach($array_list as $a){
      $prompts[] = VoipPrompt::from_array($a);
    }
    return $prompts;
  }

}
    
/**
 * Utility functions
 */

/**
 * Run an IVR menu based on the parameters provided by the given VoipCmdRunIvrMenu
 */
function voipscript_get_run_ivr_menu_script($options) {

  $script = new VoipScript('voipscript_run_ivr_menu');

  // initialize script variables
  $script->addSet('_menu_options', VoipPrompt::encode($options['prompt']));
  $input_options = $options['input_options'];
  $script->addSet('_input_options', json_encode($input_options));

  $keys_allowed = implode(array_keys($input_options));
  $script->addSet('_keys_allowed', $keys_allowed);

  $script->addSet('_counter', $options['max_attempts']);
  $script->addSet('_invalid_msg', json_encode($options['invalid_msg']));
  $script->addSet('_no_input_msg', json_encode($options['no_input_msg']));
  $script->addSet('_timeout', $options['timeout']);
  
  // run the script

  $script->addLabel('play_menu_options');
  $script->addSet('_counter', '^%_counter - 1');
  $script->addGotoIf('go_back', '^%_counter < 0');
  $script->addGetInput("^VoipPrompt::decode(%_menu_options)", 1, '', '%_timeout');
  $script->addGotoIf('no_input_received', "^%input_digits == ''");
/**
  TODO: ideally, the correct line should be as follows
  $script->addGotoIf('valid_option',
                     "^strpos(' ' . '%_keys_allowed', '%input_digits')");
   however, since the eval function is not processing '0', '*', '#' correctly,
   I had to replace the line by the following. Hopefully, we'll be able to
   solve this little bug soon (2011.04.20)
   check http://stackoverflow.com/questions/3241070/php-recursive-variable-replacement
   NOTE: among other things, the eval function adds single quotes around all 
         non-numeric strings.  Since %_keys_allowed always includes 'ti',
         we don't need to include single quotes when referring to it.
**/
  $script->addGotoIf('valid_option',
                     "^(%input_digits === '0') ? strpos(' ' . %_keys_allowed, %input_digits) : FALSE");
  $script->addGotoIf('valid_option',
                     "^(%input_digits === '*') ? strpos(' ' . %_keys_allowed, %input_digits) : FALSE");
  $script->addGotoIf('valid_option',
                     "^(%input_digits === '#') ? strpos(' ' . %_keys_allowed, %input_digits) : FALSE");
  $script->addGotoIf('valid_option',
                     "^strpos(' ' . %_keys_allowed, \"%input_digits\")");
  $script->addGoto('invalid_option');

  $script->addLabel('no_input_received');
  $script->addSet('ivr_option_selected', 
              "^_voipscript_get_element('t', json_decode(%_input_options, TRUE))");
  $script->addSay('^json_decode(%_no_input_msg, TRUE)');
  $script->addGoto('play_menu_options');
  
  $script->addLabel('valid_option');
  $script->addSet('ivr_option_selected',
      "^_voipscript_get_element(%input_digits, json_decode(%_input_options, TRUE))");
  $script->addGoto('go_back');
  
  $script->addLabel('invalid_option');
  $script->addSet('ivr_option_selected',
              "^_voipscript_get_element('i', json_decode(%_input_options, TRUE))");
  $script->addSay("^json_decode(%_invalid_msg, TRUE)");
  $script->addGoto('play_menu_options');
  
  $script->addLabel('go_back');
  $script->addUnset('_menu_options');
  $script->addUnset('_input_options');
  $script->addUnset('_keys_allowed');
  $script->addUnset('_counter');
  $script->addUnset('_invalid_msg');
  $script->addUnset('_no_input_msg');
  $script->addUnset('_timeout');
  $script->addReturn();

  return $script;
}

function voipscript_use_sms_framework() {
  if (module_exists('sms') && variable_get('voipcall_use_sms_framework', 0) == 1) {
   return TRUE;
  }
  return FALSE;
}

function voipscript_send_sms_framework($to, $text) {
  //Use SMS Framework to send SMS message if user enabled this option
  $to = str_replace('+','', $to);
  $res = sms_send($to, $text);
  watchdog('voipcall', "Sending SMS using SMS Framework. To=$to Text=$text Result=$res");
}

/**
 * Internal functions
 */

/**
 * The following function recursively replaces predefined variables from
 * inside the given string.  It was based on code made available at 
 * http://stackoverflow.com/questions/3241070/php-recursive-variable-replacement
 *
 * Notes:
 * - Strings that start with '^' are treated as expressions to be evaluated
 * - Placeholders for script variables start with '%'
 * - This function is smart enough to process variables referenced by
 *   other variables
 * - There is no check against circular inclusion, which might simply lead
 *   to an infinite loop. (Example: $vars['s'] = '%s'; ..) 
 * - When defining expressions that include string variables, make sure the
 *   variable placeholder is enclosed in " or '.  For instance,
 *     Correct form:   "^print_r('The content is ' . '%msg');"
 *     Incorrect form: "^print_r('The content is ' . %msg);"
 *   The incorrect form might produce a parser error if the variable msg
 *   contains characters such as a space, math sign, etc... It might also
 *   produce undesirable results if the variable starts with 0.
 */
function _voipscript_expand_variables($str, $vars) {
  $eval = substr($str, 0, 1) == '^';
  $regex = '/\%(\w+)/e';
  $replacement = "_voipscript_replace_variable(\$1, \$vars, \$eval);" ;
  $res = preg_replace($regex, $replacement ,$str);
  if($eval) {
    ob_start();
    $expr = substr($res, 1);
    if(eval('$res = ' . $expr . ';')===false) {
      ob_end_clean();
        watchdog('voipscript', 'Invalid PHP expression: @expr', array('@expr' => $expr), WATCHDOG_ERROR);
    }
    ob_end_clean();
  }
  return $res;
}

function _voipscript_replace_variable($var_name, $vars, $eval) {
  if(isset($vars[$var_name])) {
    $expanded = _voipscript_expand_variables($vars[$var_name], $vars);
    if($eval) {
      // Special handling since $str is going to be evaluated ..
      if(!is_numeric($expanded) || (substr($expanded . '', 0, 1)==='0'
          && strpos($expanded . '', '.')===false)) {
        $expanded = "'$expanded'";
      }
    }
    return $expanded;
  } else {
    // Variable does not exist in $vars array
    if($eval) {
      return 'null';
    }
// TODO: how to handle variables that contain strings with ', " or / ? Is there a need for a addslashes()?
//  return "$var_name";
    return '';
  }
}


function _voipscript_get_element($index, $array){
  return $array[$index];
}
