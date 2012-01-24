<?php
// $id$

/**
 * @file
 * Implementation of the VoipTropoScript class
 */


class VoipTropoScript extends VoipScript {
  
  /**
   * Constructs a new instance of the VoipTropoScript class
   */
  function __construct($name, $variables=array()) {
    parent::__construct($name, $variables);
  }
  
  /**
   * Adds a "get voice input" command to the script.
   *
   * When executed, the 'get voice input' command prompts the user for an input
   * and waits for a response via speech or touch-tone keypad.
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
   * @param $choices 
   *   String with a Simple Grammar text that tells Tropo what to expect from
   *   the user. For additional information, please check:
   *        https://www.tropo.com/docs/scripting/simple_grammar.htm
   *
   * @param $mode 
   *   String. Defines which input mode Tropo should listen for. Possible values
   *   are 'keypad', 'speech' or 'any'.
   *
   * @param $recognize_as 
   *   String. Tells Tropo what language to listen for. Examples include 'de-de'
   *   for German, 'en-us' for US English, 'es-mx' for Mexican Spanish. For a  
   *   complete list, check https://www.tropo.com/docs/webapi/ask.htm
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
   *   The 'get voice input' command sets the script variable 'input_digits' to
   *   the input received.  In case nothing is typed or the input does not 
   *   match $choices, then 'input_digits' gets set to ''
   */
  function addGetVoiceInput($prompt, $choices, $mode='speech', $recognize_as='', $end_key='#' , $timeout=5) {
    $params['prompt'] = $prompt;
    $params['choices'] = $choices;
    $params['mode'] = $mode;
    $params['recognize_as'] = $recognize_as;
    $params['timeout'] = $timeout;
    $params['end_key'] = $end_key;
    $command = new VoipCommand('VoipCmdGetVoiceInput', $params);
    parent::addCommand($command);
  }
}

