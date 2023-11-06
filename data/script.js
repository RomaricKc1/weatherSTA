$(document).ready(function () {
    $("#v_lcd_scrl").click(function () {
        var value1 = $("#lcd_scrl").val();
        $.post("lcd_scrl_spd", {
            value_lcd_scrl_spd: value1
        });
    });

    $("#v_d_slp_duration").click(function () {
        var value2 = $("#d_slp_duration").val();
        $.post("d_slp_duration", {
            value_d_slp_duration: value2
        });
    });

    $("#v_running_mode").click(function () {
        var value3 = $("#running_mode").val();
        $.post("running_mode", {
            value_running_mode: value3
        });
    });

});


function getData(url, targetElement) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            targetElement.innerHTML = this.responseText;
        }
    };
    xhttp.open("GET", url, true);
    xhttp.send();
}

//time_ds
setInterval(function () {
    getData("last_ds", document.getElementById("last_ds"));
}, 1000);
//woke up times
setInterval(function () {
    getData("wk_times", document.getElementById("wk_times"));
}, 1000);



//                  I                               N
//temp
setInterval(function () {
    getData("intemperature", document.getElementById("intemperature"));
}, 1000);

//humi
setInterval(function () {
    getData("inhumidity", document.getElementById("inhumidity"));
}, 1000);

//co2
setInterval(function () {
    getData("inco2", document.getElementById("inco2"));
}, 1000);

//temp 2
setInterval(function () {
    getData("intemperature2", document.getElementById("intemperature2"));
}, 1000);

//pressure
setInterval(function () {
    getData("inpressure", document.getElementById("inpressure"));
}, 1000);

//alt
setInterval(function () {
    getData("inaltitude", document.getElementById("inaltitude"));
}, 1000);


//              O                   U                       T
//city
setInterval(function () {
    getData("outcity", document.getElementById("outcity"));
}, 1000);

//temp
setInterval(function () {
    getData("outtemperature_now", document.getElementById("outtemperature_now"));
}, 1000);
setInterval(function () {
    getData("outtemperature_fcast", document.getElementById("outtemperature_fcast"));
}, 1000);

//temp feels like
setInterval(function () {
    getData("outfeelslike_now", document.getElementById("outfeelslike_now"));
}, 1000);
setInterval(function () {
    getData("outfeelslike_fcast", document.getElementById("outfeelslike_fcast"));
}, 1000);

//humi
setInterval(function () {
    getData("outhumidity_now", document.getElementById("outhumidity_now"));
}, 1000);
setInterval(function () {
    getData("outhumidity_fcast", document.getElementById("outhumidity_fcast"));
}, 1000);

//pressure
setInterval(function () {
    getData("outpressure_now", document.getElementById("outpressure_now"));
}, 1000);
setInterval(function () {
    getData("outpressure_fcast", document.getElementById("outpressure_fcast"));
}, 1000);

//clouds
setInterval(function () {
    getData("outclouds_now", document.getElementById("outclouds_now"));
}, 1000);
setInterval(function () {
    getData("outclouds_fcast", document.getElementById("outclouds_fcast"));
}, 1000);

//desc
setInterval(function () {
    getData("outdescription_now", document.getElementById("outdescription_now"));
}, 1000);
setInterval(function () {
    getData("outdescription_fcast", document.getElementById("outdescription_fcast"));
}, 1000);

//wind
setInterval(function () {
    getData("outwindspeed_now", document.getElementById("outwindspeed_now"));
}, 1000);
setInterval(function () {
    getData("outwindspeed_fcast", document.getElementById("outwindspeed_fcast"));
}, 1000);