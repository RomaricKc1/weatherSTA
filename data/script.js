const INTERVAL_DURATION_FAST = 10000; // 10s
const INTERVAL_DURATION_SLOW = 60000; // 1min
const OK_SUCCESSFULL = 200;
const READY_STATE_VAL = 4;

const elements_fast = [
  "last_ds",
  "wk_times",
  "intemperature",
  "inhumidity",
  "inco2",
  "intemperature2",
  "inpressure",
  "inaltitude",
];

const elements_slow = [
  "outcity",
  "outtemperature_now",
  "outtemperature_fcast",
  "outfeelslike_now",
  "outfeelslike_fcast",
  "outhumidity_now",
  "outhumidity_fcast",
  "outpressure_now",
  "outpressure_fcast",
  "outclouds_now",
  "outclouds_fcast",
  "outdescription_now",
  "outdescription_fcast",
  "outwindspeed_now",
  "outwindspeed_fcast",
  "firmware_version",
];

const buttonData = [
  {
    buttonId: "v_lcd_scrl",
    inputId: "lcd_scrl",
    postUrl: "lcd_scrl_spd",
    addr: "value_lcd_scrl_spd",
  },
  {
    buttonId: "v_d_slp_duration",
    inputId: "d_slp_duration",
    postUrl: "d_slp_duration",
    addr: "value_d_slp_duration",
  },
  {
    buttonId: "v_running_mode",
    inputId: "running_mode",
    postUrl: "running_mode",
    addr: "value_running_mode",
  },
];

////////////////////////////////////////////////////////////////////////////////
function getData(url, targetElement) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if (this.readyState == READY_STATE_VAL && this.status == OK_SUCCESSFULL) {
      if (targetElement) {
        targetElement.innerHTML = this.responseText;
      }
    }
  };
  xhttp.open("GET", url, true);
  xhttp.send();
}

function setupInterval(dataType, elementId, interval) {
  setInterval(function () {
    getData(dataType, document.getElementById(elementId));
  }, interval);
}

////////////////////////////////////////////////////////////////////////////////
elements_fast.forEach((element) => {
  setupInterval(element, element, INTERVAL_DURATION_FAST);
});

elements_slow.forEach((element) => {
  setupInterval(element, element, INTERVAL_DURATION_SLOW);
});

document.addEventListener("DOMContentLoaded", function () {
  function setupButtonHandler(buttonId, inputId, postUrl, addr) {
    const button = document.getElementById(buttonId);
    const input = document.getElementById(inputId);

    if (!button) {
      console.error(`button "${buttonId}" not found.`);
      return;
    }

    if (!input) {
      console.error(`input "${inputId}" not found.`);
      return;
    }

    button.addEventListener("click", function () {
      var value = input.value;
      var this_body = addr + "=" + encodeURIComponent(value);
      console.log(`body ${this_body}.`);

      fetch(postUrl, {
        method: "POST",
        headers: {
          "Content-Type": "application/x-www-form-urlencoded",
        },
        body: this_body,
      })
        .then((response) => {
          console.log(response);
        })
        .then((data) => {
          console.log("sent:", data);
        });
    });
  }

  buttonData.forEach((data) => {
    setupButtonHandler(data.buttonId, data.inputId, data.postUrl, data.addr);
  });

  elements_slow.forEach((element) => {
    getData(element, document.getElementById(element));
  });
  elements_fast.forEach((element) => {
    getData(element, document.getElementById(element));
  });
});
