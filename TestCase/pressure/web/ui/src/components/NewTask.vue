<template>
  <div>
    <h1>{{ msg }}</h1>
    <el-button type="primary" @click="dialogVisible = true">新建任务</el-button>
    <div>
      <el-dialog
        title="新建任务"
        :visible.sync="dialogVisible"
        width="30%"
        :close-on-click-modal="false"
      >
        <el-form ref="form" :model="form" :rules="rules" label-width="120px">
          <el-form-item label="任务名称" prop="name">
            <el-input v-model="form.name" title="任务名称长度为2~15字符" placeholder="输入任务名称, 长度为2~15字符"></el-input>
          </el-form-item>

          <el-form-item label="进程数" class="item" prop="processCount">
            <el-input-number v-model="form.processCount" :min="1" :max="10"></el-input-number>
          </el-form-item>
          <el-form-item label="数据采样率" prop="sample" class="item">
            <el-select v-model="form.sample" placeholder="数据采样率">
              <el-option
                v-for="item in form.sampleOptions"
                :key="item.value"
                :label="item.label"
                :value="item.value"
              ></el-option>
            </el-select>
          </el-form-item>
          <el-form-item label="写入数据总量" prop="expectedWrites">
            <el-input
              v-model="form.expectedWrites"
              title="例如：102400,10M,1G"
              placeholder="例如：102400,10M,1G"
            ></el-input>
          </el-form-item>
          <el-form-item label="预分配缓存" prop="cacheSize" title="每个进程预分配内存大小，确保系统内存大于[进程数×缓存大小]">
            <el-input v-model="form.cacheSize" placeholder="例如：102400,10M,1G"></el-input>
          </el-form-item>
          <el-form-item label="BFSS Server" prop="server">
            <el-input
              v-model="form.server"
              title="例如：10.0.1.119:30000"
              placeholder="例如：10.0.1.119:30000"
            ></el-input>
          </el-form-item>
          <el-form-item label="日志级别" prop="logLevel" class="item">
            <el-select v-model="form.logLevel" placeholder="日志级别">
              <el-option
                v-for="item in form.logOptions"
                :key="item.value"
                :label="item.label"
                :value="item.value"
              ></el-option>
            </el-select>
          </el-form-item>
        </el-form>
        <span slot="footer" class="dialog-footer">
          <el-button @click="dialogVisible = false">取消</el-button>
          <el-button type="primary" @click="onSubmit">确定</el-button>
        </span>
      </el-dialog>
    </div>
  </div>
</template>

<script>
import http from "@/http.js";
export default {
  name: "NewTask",
  props: {
    msg: String
  },
  data() {
    return {
      dialogVisible: this.isShow,
      form: {
        processCount: 1,
        sample: "S100K",
        sampleOptions: [
          { label: "S50K", value: "S50K" },
          { label: "S100K", value: "S100K" },
          { label: "S200K", value: "S200K" },
          { label: "S300K", value: "S300K" },
          { label: "S400K", value: "S400K" },
          { label: "S500K", value: "S500K" },
          { label: "S600K", value: "S600K" },
          { label: "S700K", value: "S700K" },
          { label: "S800K", value: "S800K" },
          { label: "S900K", value: "S900K" },
          { label: "S1M", value: "S1M" },
          { label: "S5M", value: "S5M" }
        ],
        logOptions: [
          { label: "panic", value: "panic" },
          { label: "fatal", value: "fatal" },
          { label: "error", value: "error" },
          { label: "warn", value: "warn" },
          { label: "info", value: "info" },
          { label: "debug", value: "debug" },
          { label: "trace", value: "trace" }
        ],
        logLevel: "info",
        expectedWrites: "100M",
        cacheSize: "200M",
        server: "10.0.1.119:30000"
      },

      rules: {
        name: [
          {
            required: true,
            message: "任务名不能为空",
            trigger: "blur"
          },
          {
            min: 2,
            max: 15,
            message: "任务名称长度为2~15字符",
            trigger: ["blur", "change"]
          }
        ],
        processCount: [
          {
            required: true,
            message: "进程数不能为空",
            trigger: "blur"
          }
        ],
        sample: [
          {
            required: true,
            message: "采样率不能为空",
            trigger: "blur"
          }
        ],
        expectedWrites: [
          {
            required: true,
            message: "写入数据总量格式：102400,100K,10M,1G",
            pattern: `^[1-9][0-9]*[kKmMgGtTpP]?$`,
            trigger: ["blur", "change"]
          }
        ],
        cacheSize: [
          {
            required: true,
            message: "预分配缓存：102400,100K,10M,1G",
            pattern: `^[1-9][0-9]*[kKmMgGtTpP]?$`,
            trigger: ["blur", "change"]
          }
        ],
        server: [
          {
            required: true,
            message: "URL格式为[ip:port]，例如：10.0.1.119:30000",
            // eslint-disable-next-line
            pattern: `^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\:[1-9][0-9]{2,4}$`,
            trigger: ["blur", "change"]
          }
        ],
        logLevel: [
          {
            required: true,
            message: "日志级别不能为空",
            trigger: "blur"
          }
        ]
      }
    };
  },
  methods: {
    onSubmit() {
      this.$refs["form"].validate(valid => {
        if (valid) {
          newTask(this);
        } else {
          return false;
        }
      });
    }
  }
};

function newTask(self) {
  http.newTask(
    self.form,
    data => {
      if (data.code !== "E_OK") {
        self.$message.error("新建任务失败, 原因：" + data.msg);
        return;
      }
      self.dialogVisible = false;
    },
    error => {
      self.$message.error("新建任务异常, 原因：" + error);
    }
  );
}
</script>

<!-- Add "scoped" attribute to limit CSS to this component only -->
<style scoped>
.item {
  text-align: left;
}
</style>