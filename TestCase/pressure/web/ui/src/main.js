import Vue from 'vue'
import axios from "axios"
import App from './App.vue'
import './plugins/element.js'
import router from './router'

Vue.config.productionTip = false

Vue.prototype.$http = axios;

Vue.filter('formatBytes', (bytes, decimals = 2) => {
  if (bytes === 0) return "0 Bytes";
  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ["Bytes", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"];
  let i = Math.floor(Math.log(bytes) / Math.log(k));
  if (i < 0) {
    i = 0;
  }
  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + " " + sizes[i];
});

Vue.filter('sampleString', sample => {
  let samples = [
    "S50K",
    "S100K",
    "S200K",
    "S300K",
    "S400K",
    "S500K",
    "S600K",
    "S700K",
    "S800K",
    "S900K",
    "S1M",
    "S5M",
    "S10M",
    "S100M",
    "S600M",
    "S1G",
    "S2G"
  ];
  return samples[sample];
});

Vue.filter('formatTime', seconds => {
  let t = new Date(seconds * 1000);
  let str = `${t.getFullYear()}-${String('0' + t.getMonth()).slice(-2)}-${String('0' + t.getDay()).slice(-2)}`;
  str += ` ${String('0' + t.getHours()).slice(-2)}:${String('0' + t.getMinutes()).slice(-2)}:${String('0' + t.getSeconds()).slice(-2)}`;
  return str;
});

Vue.filter('formatSeconds', seconds => {
  let hours = Math.floor(seconds / (60 * 60));
  let minutes = Math.floor((seconds % (60 * 60)) / 60);
  let s = seconds % 60;
  let str;
  if (hours > 0) {
    str = hours + '时' + minutes + '分' + s + '秒';
  } else if (minutes > 0) {
    str = minutes + '分' + s + '秒';
  } else {
    str = s + '秒';
  }
  return str;
});

new Vue({
  router,
  render: h => h(App)
}).$mount('#app')

