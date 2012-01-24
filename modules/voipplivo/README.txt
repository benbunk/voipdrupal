== Introduction ==

The voipplivo.module makes it possible for the VoIP Drupal platform to make and receive calls via the Plivo framework (http://www.plivo.org/).

Plivo is a Communications Framework to rapidly build voice based apps, to make or receive calls, using your existing web development skills and your existing infrastructure.


== Requirements ==

In order to install the voipplivo.module, you will need:

1. Configure and run Plivo and FreeSWITCH(http://www.plivo.org/get-started/)

2. The VoIP Drupal module (http://drupal.org/project/voipdrupal)

3. The PHP Curl extension in your system. For Debian systems, just run
  $ sudo apt-get install php5-curl
  $ sudo /etc/init.d/apache2 restart 


== Installation ==

Installing voipplivo.module is simple.  It requires a few configuration steps on your Drupal site to let it know how to reach your Plivo server. It also requires a few settings in your Plivo configuration to make sure it knows which Drupal site to use.


Plivo configuration:

1. Go to the /pathtoplivoinstall/etc/plivo and open default.conf in editor

2. Change this values:

  - DEFAULT_HTTP_METHOD = POST

  - DEFAULT_ANSWER_URL = http://mysite.com/voip/plivo/callhandler/ (for clean URLs) or http://mysite.com/voip/?q=plivo/callhandler/

  - EXTRA_FS_VARS = variable_duration

  - AUTH_ID = enter any value, this is your authentication id

  - AUTH_TOKEN = enter any value, this is your authentication token


3. Save the file and restart Plivo


Drupal configuration:

1. Install and enable voipplivo.module

2. Set Plivo as the default voip server

  - Go to admin/voip/servers

  - Click on Plivo's "configure" link

  - Fill in the fields "Account SID" and "Auth Token" with your Plivo "AUTH_ID" and "AUTH_TOKEN" values, respectively (see "Plivo configuration" above)
  
  - If your Plivo is on a different server than Drupal, change the value of "Plivo REST API Url" to the new server's URL
  
  - Optionally click "Plivo Outbound Call Parameters", to set up advanced options as per your needs
  
  - Press "Save". That will take you back to admin/voip/servers

  - Select the 'Plivo' option

  - Press the 'Set default voip server' button


3. Enable users to make outgoing (outbound) calls from your site

  - Go to admin/user/permissions

  - Find the "voip module" permissions

  - Enable the "make outbound calls" permission for the desired roles

  - Press the "save permissions" button   

  
== Try it out ==    

Now you should be able to call your VoIP Drupal site on  Plivo default number (1000@yourserverip:5080). Enjoy!


== About ==

The VoIP Plivo module has been originally developed by Leo Burd, Tamer Zoubi and Blair MacNeil under the sponsorship of the MIT Center for Civic Media (http://civic.mit.edu).
