<?php

/**
 * Implementation of hook_content_default_fields().
 */
function voipscriptui_content_default_fields() {
  $fields = array();

  // Exported field: field_voipscriptui_code
  $fields['voipscriptui-field_voipscriptui_code'] = array(
    'field_name' => 'field_voipscriptui_code',
    'type_name' => 'voipscriptui',
    'display_settings' => array(
      'label' => array(
        'format' => 'above',
        'exclude' => 0,
      ),
      'teaser' => array(
        'format' => 'default',
        'exclude' => 0,
      ),
      'full' => array(
        'format' => 'default',
        'exclude' => 0,
      ),
      '4' => array(
        'format' => 'default',
        'exclude' => 0,
      ),
    ),
    'widget_active' => '1',
    'type' => 'text',
    'required' => '1',
    'multiple' => '0',
    'module' => 'text',
    'active' => '1',
    'text_processing' => '1',
    'max_length' => '',
    'allowed_values' => '',
    'allowed_values_php' => '',
    'widget' => array(
      'rows' => '20',
      'size' => 60,
      'default_value' => array(
        '0' => array(
          'value' => '$script->addSay(\'your message here\');
$script->addHangup();',
          '_error_element' => 'default_value_widget][field_voipscriptui_code][0][value',
        ),
      ),
      'default_value_php' => NULL,
      'label' => 'Script code',
      'weight' => '-4',
      'description' => 'Write your script above. The variable <i>$script</i> is automatically provided.  The script will be returned whenever a module requests its name via <i>hook_voipscript_load_script($script_name)</i>.',
      'type' => 'text_textarea',
      'module' => 'text',
    ),
  );

  // Translatables
  // Included for use with string extractors like potx.
  t('Script code');

  return $fields;
}
