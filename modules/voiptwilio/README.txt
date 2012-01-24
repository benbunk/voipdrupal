== Introduction ==

The voiptwilio.module makes it possible for the VoIP Drupal platform to make and receive calls via the Twilio Cloud Communications service (http://www.twilio.com/).


== Requirements ==

In order to install the voiptwilio.module, you will need:

1. A Twilio account

2. The PHP Curl extension in your system. For Debian systems, just run
  $ sudo apt-get install php5-curl
  $ sudo /etc/init.d/apache2 restart 


== Installation ==

Installing voiptwilio.module is very simple.  It requires a few configuration steps on your Drupal site to let it know how to reach your Twilio account It also requires a few settings in your Twilio account to make sure it knows which Drupal site to use.

Drupal configuration:

1. Install and enable voiptwilio.module

2. Set Twilio as the default voip server
  - Go to admin/voip/servers

  - Click on Twilio's "configure" link

  - Fill in the fields with the "Account SID" and "Auth Token" associated with your Twilio account. Both of those values can be found in the "API Credentials" section of your account's "Dashboard"

  - Go back to admin/voip/servers

  - Select the 'Twilio' option

  - Press the 'set default voip server' button


Twilio configuration:

1. Login into your Twilio account

2. In the "Numbers" section of the account, click on the "Edit" link associated with the phone number you would like to use for your Drupal site

3. Select the Twilio API version to be used
  - Click on "Advanced Properties" at the bottom of the page
  - Set the "API Version" field to "2010-04-01" 

4. Set the URLs associated with your site
  - Mark the "Voice" check box

  - Fill the "URL" field with
    http://mysite.com/voip/twilio/callhandler/process_inbound_calls/ (for clean URLs)
    or http://mysite.com/?q=voip/twilio/callhandler/process_inbound_calls/
    
  - Fill the "SMS URL" field with
    http://mysite.com/voip/twilio/callhandler/process_inbound_text/ (for clean URLs)
    or http://mysite.com/?q=voip/twilio/callhandler/process_inbound_text/

  - Click on "Advanced Features"

  - Fill the "StatusCallback URL" field with
    http://mysite.com/voip/twilio/callhandler/process_hangup/ (for clean URLs)
    or http://mysite.com/?q=voip/twilio/callhandler/process_hangup/

  - Make sure both "URL" and "StatusCallback URL" are set to use "POST"

  - Press the "Save" button

  - Enjoy!


== About ==

This module was originally designed and implemented by Leo Burd from the MIT Center for Civic Media (http://civic.mit.edu/). 
