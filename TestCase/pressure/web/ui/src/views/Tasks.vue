<template>
  <div class="home">
    <NewTask msg="Welcome to bfss-pressure"/>
    <div>
      <el-button type="success" style="margin-top:20px" title="启动所有未开始任务" @click="startAllTask()">启动所有任务</el-button>
      <el-button type="warning" style="margin-top:20px" title="导出所有已完成任务">
        <a v-bind:href="exportUrl" style="display:inline-block">导出所有任务</a>
      </el-button>
    </div>

    <Task></Task>
  </div>
</template>

<script>
// @ is an alias to /src
import NewTask from "@/components/NewTask.vue";
import Task from "@/components/Task.vue";
import http from "@/http.js";

export default {
  name: "tasks",
  components: {
    NewTask,
    Task
  },
  data() {
    return {
      exportUrl: http.exportFullUrl
    };
  },
  methods: {
    startAllTask: function() {
      let self = this;
      http.startAllTask(data => {
        if(data.code !== 'E_OK') {
          self.$message.error('启动全部任务失败，原因：'+ data.msg);
          return
        }
        self.$message.success('启动全部任务成功');
      }, error => {
        self.$message.error('启动全部任务异常，原因：'+ error);
      })
    }
  }
};
</script>

<style scoped>
a:link,
a:visited,
a:active,
a:hover {
  text-decoration: none;
}
a {
  color: #fff;
}
</style>
