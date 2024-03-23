/*


  OK, ya ready for some fun? HTML + CSS styling + javascript all in and undebuggable environment

  one trick I've learned to how to debug HTML and CSS code.

  get all your HTML code (from html to /html) and past it into this test site
  muck with the HTML and CSS code until it's what you want
  https://www.w3schools.com/html/tryit.asp?filename=tryhtml_intro

  No clue how to debug javascrip other that write, compile, upload, refresh, guess, repeat

  I'm using class designators to set styles and id's for data updating
  for example:
  the CSS class .tabledata defines with the cell will look like
  <td><div class="tabledata" id = "switch"></div></td>

  the XML code will update the data where id = "switch"
  java script then uses getElementById
  document.getElementById("switch").innerHTML="Switch is OFF";


  .. now you can have the class define the look AND the class update the content, but you will then need
  a class for every data field that must be updated, here's what that will look like
  <td><div class="switch"></div></td>

  the XML code will update the data where class = "switch"
  java script then uses getElementsByClassName
  document.getElementsByClassName("switch")[0].style.color=text_color;


  the main general sections of a web page are the following and used here

  <html>
    <style>
    // dump CSS style stuff in here
    </style>
    <body>
      <header>
      // put header code for cute banners here
      </header>
      <main>
      // the buld of your web page contents
      </main>
      <footer>
      // put cute footer (c) 2021 xyz inc type thing
      </footer>
    </body>
    <script>
    // you java code between these tags
    </script>
  </html>


*/

// note R"KEYWORD( html page code )KEYWORD"; 
// again I hate strings, so char is it and this method let's us write naturally

const char PAGE_MAIN[] PROGMEM = R"=====(

<!DOCTYPE html>
<html lang="en" class="js-focus-visible">

<title>Microscope Stage Controller</title>

  <style>
    table {
      position: relative;
      width:100%;
      border-spacing: 0px;
    }
    tr {
      border: 1px solid white;
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 20px;
    }
    th {
      height: 20px;
      padding: 3px 15px;
      background-color: #343a40;
      color: #FFFFFF !important;
      }
    td {
      height: 20px;
       padding: 3px 15px;
    }
    .tabledata {
      font-size: 24px;
      position: relative;
      padding-left: 5px;
      padding-top: 5px;
      height:   25px;
      border-radius: 5px;
      color: #FFFFFF;
      line-height: 20px;
      transition: all 200ms ease-in-out;
      //background-color: #00AA00;
    }
    .move_input {
      width: 05%;
      height: 55px;
      outline: none;
      height: 25px;
    }
    .bodytext {
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 24px;
      text-align: left;
      font-weight: light;
      border-radius: 5px;
      display:inline;
    }
    .navbar {
      width: 100%;
      height: 50px;
      margin: 0;
      padding: 10px 0px;
      background-color: #FFF;
      color: #000000;
      border-bottom: 5px solid #293578;
    }
    .fixed-top {
      position: fixed;
      top: 0;
      right: 0;
      left: 0;
      z-index: 1030;
    }
    .navtitle {
      float: left;
      height: 50px;
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 50px;
      font-weight: bold;
      line-height: 50px;
      padding-left: 20px;
    }
   .navheading {
     position: fixed;
     left: 60%;
     height: 50px;
     font-family: "Verdana", "Arial", sans-serif;
     font-size: 20px;
     font-weight: bold;
     line-height: 20px;
     padding-right: 20px;
   }
   .navdata {
      justify-content: flex-end;
      position: fixed;
      left: 70%;
      height: 50px;
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 20px;
      font-weight: bold;
      line-height: 20px;
      padding-right: 20px;
   }
    .category {
      font-family: "Verdana", "Arial", sans-serif;
      font-weight: bold;
      font-size: 32px;
      line-height: 50px;
      padding: 20px 10px 0px 10px;
      color: #000000;
    }
    .heading {
      font-family: "Verdana", "Arial", sans-serif;
      font-weight: normal;
      font-size: 28px;
      text-align: left;
    }
  
    .btn {
      background-color: #444444;
      border: none;
      color: white;
      padding: 10px 20px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      margin: 14px 12px;
      cursor: pointer;
    }
    .foot {
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 20px;
      position: relative;
      height:   30px;
      text-align: center;   
      color: #AAAAAA;
      line-height: 20px;
    }
    .container {
      max-width: 1800px;
      margin: 0 auto;
    }
    table tr:first-child th:first-child {
      border-top-left-radius: 5px;
    }
    table tr:first-child th:last-child {
      border-top-right-radius: 5px;
    }
    table tr:last-child td:first-child {
      border-bottom-left-radius: 5px;
    }
    table tr:last-child td:last-child {
      border-bottom-right-radius: 5px;
    }
    
  </style>

  <body style="background-color: #efefef" onload="process()">
  
    <header>
      <div class="navbar fixed-top">
          <div class="container">
            <div class="navtitle">Microscope Stage Controller</div>
            <div class="navdata" id = "date">mm/dd/yyyy</div>
            <div class="navheading">DATE</div><br>
            <div class="navdata" id = "time">00:00:00</div>
            <div class="navheading">TIME</div>
            
          </div>
      </div>
    </header>
  
    <main class="container" style="margin-top:70px">
      <div class="category">Parameters</div>
      <br>
      <div style="border-radius: 10px !important;">
      <table style="width:50%">
      <colgroup>
        <col span="1" style="background-color:rgb(230,230,230); width: 20%; color:#000000 ;">
        <col span="1" style="background-color:rgb(200,200,200); width: 15%; color:#000000 ;">
        <col span="1" style="background-color:rgb(180,180,180); width: 15%; color:#000000 ;">
        <col span="1" style="background-color:rgb(150,150,150); width: 15%; color:#000000 ;">
      </colgroup>
      <!-- 
      <col span="2"style="background-color:rgb(0,0,0); color:#FFFFFF">
      <col span="2"style="background-color:rgb(0,0,0); color:#FFFFFF">
      <col span="2"style="background-color:rgb(0,0,0); color:#FFFFFF">
      <col span="2"style="background-color:rgb(0,0,0); color:#FFFFFF"> -->
      <tr>
        <th colspan="1"><div class="heading"></div></th>
        <th colspan="1"><div class="heading">x</div></th>
        <th colspan="1"><div class="heading">y</div></th>
        <th colspan="1"><div class="heading">z</div></th>
      </tr>
      <tr>
        <td><div class="bodytext">Position</div></td>
        <td><div class="tabledata" id = "x1"></div></td>
        <td><div class="tabledata" id = "y1"></div></td>
        <td><div class="tabledata" id = "z1"></div></td>
      </tr>
      </table>
    </div>
    <br>
    <div class="category">Driver</div>
    <br>
    <div class="bodytext">Move Stage [steps] </div>
    <br>
    <br>
    <div class="bodytext">x:  </div>
    <input type="number" class="move_input" id = "x_steps" value = "0" width = "0%" "/>
    <br>
    <div class="bodytext">y:  </div>
    <input type="number" class="move_input" id = "y_steps" value = "0" width = "0%" "/>
    <br>
    <div class="bodytext">z:  </div>
    <input type="number" class="move_input" id = "z_steps" value = "0" width = "0%" "/>
    <br>
    <br>
    <button type="button" name = "move" class = "btn" id = "btn0" onclick="ButtonPress0(x_steps.value, y_steps.value, z_steps.value)">move</button>
    </div>
    <br>
    <br>
    <br>
    <div class="bodytext">Measure scan range [steps]</div>
    <br>
    <br>
    <table style="width:50%">
      <colgroup>
        <col span="1" style="background-color:rgb(230,230,230); width: 15%; color:#000000 ;">
        <col span="1" style="background-color:rgb(200,200,200); width: 15%; color:#000000 ;">
        <col span="1" style="background-color:rgb(180,180,180); width: 15%; color:#000000 ;">
        <col span="1" style="background-color:rgb(150,150,150); width: 15%; color:#000000 ;">
        <col span="1" style="background-color:rgb(0,100,0); width: 15%; color:#000000 ;">
        <col span="1" style="background-color:rgb(0,100,0); width: 15%; color:#000000 ;">
      </colgroup>
      <tr>
        <th colspan="1"><div class="heading"></div></th>
        <th colspan="1"><div class="heading">start</div></th>
        <th colspan="1"><div class="heading">end</div></th>
        <th colspan="1"><div class="heading">diff</div></th>
        <th colspan="1"><div class="heading">frame size</div></th>
        <th colspan="1"><div class="heading">scan range</div></th>
      </tr>
      <tr>
        <td><div class="bodytext">x</div></td>
        <td><div class="tabledata" id = "xs"></div></td>
        <td><div class="tabledata" id = "xe"></div></td>
        <td><div class="tabledata" id = "xd"></div></td>
        <td><div class="tabledata" id = "xfs"></div></td>
        <td><div class="tabledata" id = "xsr"></div></td>
      </tr>
      <tr>
        <td><div class="bodytext">y</div></td>
        <td><div class="tabledata" id = "ys"></div></td>
        <td><div class="tabledata" id = "ye"></div></td>
        <td><div class="tabledata" id = "yd"></div></td>
        <td><div class="tabledata" id = "yfs"></div></td>
        <td><div class="tabledata" id = "ysr"></div></td>
      </tr>
      <tr>
        <td><div class="bodytext">z</div></td>
        <td><div class="tabledata" id = "zs"></div></td>
        <td><div class="tabledata" id = "ze"></div></td>
        <td><div class="tabledata" id = "zd"></div></td>
        <td><div class="tabledata" id = "zfs"></div></td>
        <td><div class="tabledata" id = "zsr"></div></td>
      </tr>
      
      </table>
    <br>
    <button type="button" class = "btn" id = "btn1" onclick="ButtonPress1()">Measure start</button>
    <button type="button" class = "btn" id = "btn2" onclick="set_frame_size()">Set frame size</button>
    <button type="button" class = "btn" id = "btn3" onclick="set_scan_range()">Set scan range</button>
            <button type="button" class = "btn" id = "btn4" onclick="scan()" style="background-color: #006400;" >Scan</button>
    </div>
    <br>
    <br>
    
  </main>

  <footer div class="foot" id = "temp" >ESP32 Web Page Creation and Data Update Demo</div></footer>
  
  </body>


  <script type = "text/javascript">
  
    // global variable visible to all java functions
    var xmlHttp=createXmlHttpObject();

    // function to create XML object
    function createXmlHttpObject(){
      if(window.XMLHttpRequest){
        xmlHttp=new XMLHttpRequest();
      }
      else{
        xmlHttp=new ActiveXObject("Microsoft.XMLHTTP");
      }
      return xmlHttp;
    }

    // function to handle button press from HTML code above
    // and send a processing string back to server
    // this processing string is use in the .on method
    
    function ButtonPress0(value1, value2, value3) {
      var xhttp = new XMLHttpRequest(); 
      var message;

      //document.getElementById("btn0").style.background = '#444444';
      // if you want to handle an immediate reply (like status from the ESP
      // handling of the button press use this code
      // since this button status from the ESP is in the main XML code
      // we don't need this
      // remember that if you want immediate processing feedbac you must send it
      // in the ESP handling function and here
      /*
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          message = this.responseText;
          // update some HTML data
        }
      }
      */
       
      var variables = "x_steps=" + value1 + "&" + "y_steps=" + value2 + "&" + "z_steps=" + value3;
      xhttp.open("PUT", "B_MOVE?"+variables , true);
      xhttp.send();
    }

    function ButtonPress1() {
      var xhttp = new XMLHttpRequest(); 

      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          // update the web based on reply from  ESP
          document.getElementById("btn1").innerHTML=this.responseText;
        }
      }
      
      xhttp.open("PUT", "B_MEASURE", false);
      xhttp.send(); 
    }

    function set_frame_size() {
      var xhttp = new XMLHttpRequest(); 
      
      xhttp.open("PUT", "B_SETFRAME", false);
      xhttp.send(); 
    }

    function set_scan_range() {
      var xhttp = new XMLHttpRequest(); 
      
      xhttp.open("PUT", "B_SETFOCUS", false);
      xhttp.send(); 
    }

    function scan() {
      var xhttp = new XMLHttpRequest(); 
      
      xhttp.open("PUT", "B_SCAN", false);
      xhttp.send(); 
    }
    
    function UpdateSlider(value) {
      var xhttp = new XMLHttpRequest();
      // this time i want immediate feedback to the fan speed
      // yea yea yea i realize i'm computing fan speed but the point
      // is how to give immediate feedback
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          // update the web based on reply from  ESP
          document.getElementById("fanrpm").innerHTML=this.responseText;
        }
      }
      // this syntax is really weird the ? is a delimiter
      // first arg UPDATE_SLIDER is use in the .on method
      // server.on("/UPDATE_SLIDER", UpdateSlider);
      // then the second arg VALUE is use in the processing function
      // String t_state = server.arg("VALUE");
      // then + the controls value property
      xhttp.open("PUT", "UPDATE_SLIDER?VALUE="+value, true);
      xhttp.send();
    }

    // function to handle the response from the ESP
    function response(){
      var message;
      var barwidth;
      var currentsensor;
      var xmlResponse;
      var xmldoc;
      var dt = new Date();
      var color = "#e8e8e8";
     
      // get the xml stream
      xmlResponse=xmlHttp.responseXML;
  
      // get host date and time
      document.getElementById("time").innerHTML = dt.toLocaleTimeString();
      document.getElementById("date").innerHTML = dt.toLocaleDateString();
  
      // X0
      xmldoc = xmlResponse.getElementsByTagName("X0"); //bits for A0
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("x1").innerHTML=message;
      
      // Y0
      xmldoc = xmlResponse.getElementsByTagName("Y0"); //volts for A0
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("y1").innerHTML=message;
  
      // Z0
      xmldoc = xmlResponse.getElementsByTagName("Z0");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("z1").innerHTML=message;
      
      // XS
      xmldoc = xmlResponse.getElementsByTagName("XS");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("xs").innerHTML=message;
      
      // XE
      xmldoc = xmlResponse.getElementsByTagName("XE");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("xe").innerHTML=message;
  
      // XD
      xmldoc = xmlResponse.getElementsByTagName("XD");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("xd").innerHTML=message;

      // XFS
      xmldoc = xmlResponse.getElementsByTagName("XFS");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("xfs").innerHTML=message;

      // XSR
      xmldoc = xmlResponse.getElementsByTagName("XSR");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("xsr").innerHTML=message;

      // YS
      xmldoc = xmlResponse.getElementsByTagName("YS");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("ys").innerHTML=message;
      
      // YE
      xmldoc = xmlResponse.getElementsByTagName("YE");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("ye").innerHTML=message;
  
      // YD
      xmldoc = xmlResponse.getElementsByTagName("YD");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("yd").innerHTML=message;

      // YFS
      xmldoc = xmlResponse.getElementsByTagName("YFS");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("yfs").innerHTML=message;

      // YSR
      xmldoc = xmlResponse.getElementsByTagName("YSR");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("ysr").innerHTML=message;

      // ZS
      xmldoc = xmlResponse.getElementsByTagName("ZS");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("zs").innerHTML=message;
      
      // ZE
      xmldoc = xmlResponse.getElementsByTagName("ZE");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("ze").innerHTML=message;
  
      // ZD
      xmldoc = xmlResponse.getElementsByTagName("ZD");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("zd").innerHTML=message;

      // ZFS
      xmldoc = xmlResponse.getElementsByTagName("ZFS");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("zfs").innerHTML=message;

      // ZSR
      xmldoc = xmlResponse.getElementsByTagName("ZSR");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("zsr").innerHTML=message;
     }
  
    // general processing code for the web page to ask for an XML steam
    // this is actually why you need to keep sending data to the page to 
    // force this call with stuff like this
    // server.on("/xml", SendXML);
    // otherwise the page will not request XML from the ESP, and updates will not happen
    function process(){
     
     if(xmlHttp.readyState==0 || xmlHttp.readyState==4) {
        xmlHttp.open("PUT","xml",true);
        xmlHttp.onreadystatechange=response;
        xmlHttp.send(null);
      }       
        // you may have to play with this value, big pages need more porcessing time, and hence
        // a longer timeout
        setTimeout("process()",100);
    }
  
  
  </script>

</html>



)=====";
