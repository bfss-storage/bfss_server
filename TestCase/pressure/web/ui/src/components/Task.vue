<template>
  <div class="task-container">
    <section>
      <el-row type="flex" justify="flex-start" class="task" :gutter="20">
        <el-col :span="8" v-for="task in tasks" :key="task.name">
          <el-card class="box-card">
            <div slot="header" class="clearfix">
              <span style="display:inline-block; margin-top:13px;">{{task.name}}</span>
              <span class="action" style="float: right; padding-bottom: 12px">
                <i
                  v-if="[1,2].includes(task.status)"
                  style="font-size: 15px; margin-right:10px; color:#E6A23C"
                >{{task.speed | formatBytes}}/s</i>
                <i
                  v-if="task.status === 3"
                  style="font-size: 15px; margin-right:10px; color:#E6A23C"
                >Waiting</i>
                <i
                  class="el-icon-loading"
                  v-if="task.status === 1"
                  style="font-size:24px; color:#409EFF;"
                  title="运行中"
                ></i>
                <el-button type="text" v-if="task.status === 0" @click="startTask(task.name)">启动</el-button>
                <el-button type="text" v-if="task.status === 2">
                  <a v-bind:href="exportUrl+ '?name='+task.name">导出报表</a>
                </el-button>
              </span>
            </div>
            <div style="height: 30px">
              <el-progress type="line" :width="60" :percentage="task.progress"></el-progress>
            </div>
            <div class="item">
              <div>
                <span class="label">采样率：</span>
                {{ task.cfg.sample | sampleString}}
              </div>
              <div>
                <span class="label">预分配缓存：</span>
                {{ task.cfg.cache_size | formatBytes }}
              </div>
              <div>
                <span class="label">写入数据总量：</span>
                {{ task.cfg.expected_writes | formatBytes }}
              </div>
              <div>
                <span class="label">BFSS Server：</span>
                {{task.cfg.server}}
              </div>
            </div>
          </el-card>
        </el-col>
      </el-row>
    </section>
  </div>
</template>

<script>
import http from "@/http.js";

export default {
  name: "Task",
  data() {
    return {
      tasks: [],
      exportUrl: http.exportFullUrl
    };
  },
  methods: {
    startTask: function(name) {
      let self = this;
      http.startTask(
        name,
        data => {
          if (data.code !== "E_OK") {
            self.$message.error("启动失败, 原因：" + data.msg);
            return;
          }
          self.$message.success("启动成功");
        },
        error => {
          self.$message.error("启动异常, 原因：" + error);
        }
      );
    },
    queryTasks: function() {
      let self = this;
      http.queryAllTasks(
        data => {
          if (data.code === "E_OK") {
            self.tasks = data.data;
          }
        },
        error => {
          // eslint-disable-next-line
          console.log(error);
        }
      );
    }
  },
  created: function() {
    let s = this;
    s.timer = setInterval(this.queryTasks, 1000 * 2);
  }
};
</script>

<style scoped>
.task-container {
  display: flex;
  flex-direction: row;
  justify-content: center;
}
section {
  margin-top: 20px;
  width: 1200px;
}
.task {
  flex-wrap: wrap;
}
.action {
  display: inline-block;
  height: 30px;
}
.box-card {
  width: 350px;
  margin-bottom: 20px;
}
.box-card::last-child {
  margin-bottom: 0;
}
.clearfix:before,
.clearfix:after {
  display: table;
  content: "";
}
.clearfix:after {
  clear: both;
}
.clearfix {
  text-align: left;
}
a:link,
a:visited,
a:active {
  text-decoration: none;
}
a:hover {
  text-decoration: underline;
}
a {
  color: #409eff;
}
.item .label {
  display: inline-block;
  width: 100px;
  text-align: right;
  margin-right: 10px;
  margin-bottom: 5px;
}
.item {
  font-size: 14px;
  text-align: left;
}
</style>


