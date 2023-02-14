defineRule({
    whenChanged: "ad-sv_1/TVOC", // если значение сенсора изменилось
    then: function(newValue, devName, cellName) {
    if (newValue >2000) {
        dev["buzzer/enabled"] = true;
    } else {
        dev["buzzer/enabled"] = false;
    }
    }
});
