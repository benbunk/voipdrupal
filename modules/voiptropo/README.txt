== Introduction ==

The VoIP Tropo module makes it possible for the VoIP Drupal platform to make and receive text and voice calls via the Tropo Cloud API service (http://www.tropo.com/).

In particular, the VoIP Tropo module enables the creation of Drupal sites that:
* Are accessible via Skype, IM, Twitter, SIP connections and phone numbers in over 40 countries
* Provide SMS capabilities to and from U.S. numbers
* Can use a combination of 16 voices (8 male, 8 female) in 6 different languages for text to speech generation

In addition to that, the VoIP Tropo module extends the standard VoIP Drupal API with support to voice recognition via the new addGetVoiceInputCommand(). Check the 'voiptropo_speech_recognition_demo' script for an example of that functionality in action. 


== Requirements ==

In order to install the voiptropo.module, you will need:

1. A Tropo account

2. The VoIP Drupal module (http://drupal.org/project/voipdrupal)

3. The PHP Curl extension in your system. For Debian systems, just run
  $ sudo apt-get install php5-curl
  $ sudo /etc/init.d/apache2 restart 


== Installation ==

Installing voiptropo.module is very simple.  It requires a few configuration steps on your Drupal site to let it know how to reach your Tropo account. It also requires a few settings in your Tropo account to make sure it knows which Drupal site to use.

Tropo configuration:

1. Login into your Tropo account


2. Create a new Tropo application for your site

  - In the "Your Applications" section of the account, click on the "Create new application" link and choose Tropo WebAPI.

  - Fill the application name with a name of choice

  - Fill the "URL" field with
    http://mysite.com/voip/tropo/callhandler/ (for clean URLs)
    or http:// mysite.com/voip/?q=tropo/callhandler/

  - Press the "Create application" button


3. Fill in the additional options to activate your application tokens

  - Click on "Outbound Tokens" -> "Voice" and a pop-up window will appear. Press the "Launch Token" button. 

  - Repeat same process for the Messaging token.

  - NOTE: Those are the 2 tokens that you should copy to the Tropo configuration in your Drupal site.


4. Associate a Tropo phone number with your site
  
  By default, Tropo provides a Skype Voice number, a SIP Voice address and a Phono (http://phono.com/) app address that might be used to reach your site. In case you want to associate an actual phone number with your site, follow the steps below:

  - Click on "Add new phone number". A pop-up window will come up.

  - Choose a phone number and press the plus ('+') sign on the right. This will be the inbound number for your site.

  - Close the pop-up window. You new number(s) will be displayed in your Tropo application's settings
 
  - NOTE: If you want this number to be used as the Caller Id in the calls made by your site, then 
    a) go to your site's admin/voip/call section 
    b) Set the "Caller id number" field with the new number. ATTENTION: That field only takes numeric digits (ie. 0013523537528).


5. Associate an IM, GTalk, MSN, Twitter, or Yahoo account with your site

  - Click on the desired icon under the "Instant Messaging Networks" section of your Tropo application

  - Fill in the required fields with the user id and password associated with your Drupal site

  - Click the "activate" button on the right


Drupal configuration:

1. Install and enable voiptropo.module

2. Set Tropo as the default voip server

  - Go to admin/voip/servers

  - Click on Tropo's "configure" link

  - Fill in the fields with the "Username", "Password" with your Tropo credentials

  - Fill in the "Voice Outbound Token" and "Messaging Outbound Token" with the tokens associated with the Tropo application that you want to connect with your site (see "Tropo configuration" above).

  - Press "Save". That will take you back to admin/voip/servers

  - Select the 'Tropo' option

  - Press the 'Set default voip server' button


3. Enable users to make outgoing (outbound) calls from your site

  - Go to admin/user/permissions

  - Find the "voip module" permissions

  - Enable the "make outbound calls" permission for the desired roles

  - Press the "save permissions" button 


== Try it out ==    

Now you should be able to call your VoIP Drupal site on any of the numbers listed under "Phone Numbers" in your Tropo application. Enjoy!


== About ==

The VoIP Tropo module has been originally developed by Leo Burd and Tamer Zoubi under the sponsorship of the MIT Center for Civic Media (http://civic.mit.edu).
