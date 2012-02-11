<?php
// $Id$

/**
 * Implments hook_form().
 */
function voip_admin_default_form($form, &$form_state) {
  $header = array(
    'name' => array('data' => t('Name')),
    'operations' => array('data' => t('Operations')),
  );

  $server_array = VoipServer::getServers();

  // sort the array by server name
  usort($server_array, "_server_cmp");

  $options = array();
  foreach ($server_array as $server) {
    $id = $server->getId();
    $name = $server->getName();

    if (function_exists($server->getConfigForm())) {
      $operations = l(t('Configure'), 'admin/voip/servers/' . $id);
    }
    else {
      $operations = t('No configuration options');
    }

    $options[$id] = array(
      '#id' => $id,
      '#value' => $id,
      'data' => array(
        '#title' => t($name),
        '#id' => $id,
      ),
      'name' => t($name),
      'operations' => $operations,
    );
  }

  $form['default'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $options,
    '#default_value' => variable_get('voip_default_server', 'log'),
    //'#js_select' => FALSE,
    '#multiple' => FALSE,
    '#empty' => t('No Servers Installed.'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Set default voip server'),
  );

  return $form;
}

function _server_cmp($s1, $s2) {
  $name1 = $s1->getName();
  $name2 = $s2->getName();
  if ($name1 == $name2) {
    return 0;
  }
  return ($name1 < $name2) ? -1 : 1;
}

/**
 * Implements hook_form_submit().
 */
function voip_admin_default_form_submit($form, &$form_state) {
  // Process form submission to set the default voip server
  if ($form_state['values']['default']) {
    drupal_set_message(t('Default voip server updated.'));
    variable_set('voip_default_server', $form_state['values']['default']);
  }
}

/**
 * implements hook_form().
 */
function voip_admin_server_form($form, &$form_state, $server_id) {
  $server = VoipServer::getServer($server_id);
  $config_form = $server->getConfigForm();
  $config = $server->getConfig();
  if ($server && function_exists($config_form)) {
    drupal_set_title(t('@server configuration', array('@server' => $server->getName())), PASS_THROUGH);
    $form = $config_form($config);

    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );
    $form['server'] = array(
      '#type' => 'value',
      '#value' => $server,
    );

    return $form;
  }
  drupal_not_found();
  exit();
}

/**
 * Implements hook_form_validate().
 *
 * Validation for voip server configuration. If the server defines a validation
 * function it uses that.
 */
function voip_admin_server_form_validate($form, &$form_state) {
  // Pass validation to voip server
  $config_form = $form_state['values']['server']->getConfigForm();
  $function = $config_form . '_validate';
  if (function_exists($function)) {
    $function($form, $form_state);
  }
}

/**
 * Implements hook_form_submit().
 */
function voip_admin_server_form_submit($form, &$form_state) {
  $server = $form_state['values']['server'];

  // Remove unecessary values
  unset($form_state['values']['op'], $form_state['values']['submit'], $form_state['values']['server'], $form_state['values']['form_token'], $form_state['values']['form_id']);

  //Save VoIP Voice
  VoipVoice::setDefaultVoice($form_state['values']['voipcall_voice_id']);
  VoipVoice::setDefaultManVoice($form_state['values']['voipcall_man_voice_id']);
  VoipVoice::setDefaultWomanVoice($form_state['values']['voipcall_woman_voice_id']);
  unset($form_state['values']['voipcall_voice_id'], $form_state['values']['voipcall_man_voice_id'], $form_state['values']['voipcall_woman_voice_id']);

  // store server configuration
  $server_id = $server->getId();
  variable_set('voip_' . $server_id . '_settings', $form_state['values']);
  drupal_set_message(t('The server settings have been saved.'));

  $form_state['redirect'] = 'admin/voip/servers';
}

