<?php

/*
 * Basic script that enables users to subscribe, unsubscribe and broadcast messages to SMS groups.
 *
 * @return FALSE in case of failure, or the script array in case of success
 */
function _voipscriptsamples_sms_group(){
  $script = new VoipScript('voipscript_sms_group');
  $script->addSet('request_result', '^voipscriptsamples_sms_group_parse_request(%inbound_text_contents, %caller_number)');
  $script->addSendText("%request_result");
  $script->addHangup('reset');
  return $script;
}
function voipscriptsamples_sms_group_parse_request($msg, $from) {
  $msg = trim($msg);
  if (!$msg) {
    $ret = t("Empty request received");
  }
  else {
    $request = explode(" ", $msg);
    $cmd = strtoupper($request[0]);
    if ($cmd == '?') {
      $ret = t("Available commands: a) S X (subscribe to group X); b) B X msg (broadcast message msg to group X); c) U X (unsubscribe to group X); d) ? (this help message)");
    }
    else if (count($request)<2) {
      $ret = t("Invalid request: $msg");
    }
    else {
      $group_id = $request[1];
      switch ($cmd) {
        default:
          $ret = t("Invalid request: $msg");
          break;
        case "S":
          $group = variable_get("voip_sms_group_".$group_id, array());
          $group[(string)$from] = $from;
          variable_set("voip_sms_group_".$group_id, $group);
          $ret = t("You are now subscribed to group ").$group_id;
          break;
        case "B":
          //Broadcast the message
          unset($request[0]);
          unset($request[1]);
          $text = implode(" ", $request);
            if(empty($text)) {
            return t("Message can't be empty");
          }
          $group = variable_get("voip_sms_group_".$group_id, array());
          foreach ($group as $number) {
            if ($number != $from) {
              $call = new VoipCall();
              $call->setCurrentChannel('text');
              $call->setCurrentNetwork('sms');
              $call->setDestNumber($number);
              $call->setCallerNumber(variable_get('voipcall_cid_number', NULL));
              voip_text($text, $call);
            }
          }
          $options = array('@gid' => $group_id);
          $ret = t("Your message has been broadcasted to group @gid", $options);
          break;
        case "U":
          $group = variable_get("voip_sms_group_".$group_id, array());
          unset($group[(string)$from]);
          variable_set("voip_sms_group_".$group_id, $group);
          $ret = t("You have been unsubscribed from group ").$group_id;
          break;
      }
    }
  }
  return $ret;
}
