import axios from 'axios'

let baseUrl = process.env.NODE_ENV === 'production' ? 'http://10.0.1.181:9527' : 'http://localhost:9527';
let tasksUrl = '/api/v1/tasks';
let exportUrl = tasksUrl + '/export';
let taskStartUrl = tasksUrl + '/start';
let taskSummary = tasksUrl + '/summary';

const axiosIns = axios.create({
  baseURL: baseUrl
});

function httpCall(config, call, errCall) {
  axiosIns.request(config).then(data => {
    if (call) {
      call(data.data);
    }
  }).catch(error => {
    if (errCall) {
      errCall(error);
    }
  })
}

export default {
  baseUrl: baseUrl,
  exportFullUrl: baseUrl + exportUrl,
  queryAllTasks: function (call, errCall) {
    httpCall({ method: 'get', url: tasksUrl }, call, errCall);
  },
  startTask: (name, call, errCall) => {
    httpCall({ method: 'post', url: taskStartUrl + '?name=' + name }, call, errCall);
  },
  startAllTask: (call, errCall) => {
    httpCall({ method: 'post', url: taskStartUrl }, call, errCall);
  },
  newTask: (form, call, errCall) => {
    httpCall({
      method: 'post', url: tasksUrl, data: {
        name: form.name,
        process_count: form.processCount,
        sample: form.sample,
        expected_writes: form.expectedWrites,
        server: form.server,
        cache_size: form.cacheSize,
        log_level: form.logLevel
      }
    }, call, errCall);
  },
  queryTaskSummary: (call, errCall) => {
    httpCall({ method: 'get', url: taskSummary }, call, errCall);
  }
};
