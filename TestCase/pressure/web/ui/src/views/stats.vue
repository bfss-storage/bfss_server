<template>
  <div>
    <h1>**** 统计 ****</h1>
    <div>
      <div style="margin-bottom: 20px;">
        <el-table
          ref="multipleTable"
          :data="tableData"
          style="width: 100%; overflow: auto;"
          max-height= "500px"
          @selection-change="handleSelectionChange"
        >
          <el-table-column type="selection" width="55"></el-table-column>
          <el-table-column property="name" label="任务名称" width="150"></el-table-column>
          <el-table-column property="sample" label="采样率" width="100">
            <template slot-scope="scope">
              <span>{{scope.row.sample | sampleString}}</span>
            </template>
          </el-table-column>
          <el-table-column property="total_writes" label="写入数据总量" width="120">
            <template slot-scope="scope">
              <span>{{scope.row.total_writes | formatBytes}}</span>
            </template>
          </el-table-column>
          <el-table-column property="speed" label="写入速度" width="120" align="center">
            <template slot-scope="scope">
              <span>{{scope.row.speed | formatBytes}}/s</span>
            </template>
          </el-table-column>
          <el-table-column property="process_count" label="进程数" width="100" align="center"></el-table-column>
          <el-table-column property="total_files" label="文件总数" width="100" align="center"></el-table-column>
          <el-table-column label="运行时长" width="120" align="center">
            <template slot-scope="scope">
              <span>{{(scope.row.done_at - scope.row.started_at)|formatSeconds}}</span>
            </template>
          </el-table-column>
          <el-table-column property="started_at" label="开始时间">
            <template slot-scope="scope">
              <span>{{scope.row.started_at | formatTime}}</span>
            </template>
          </el-table-column>
        </el-table>
      </div>
      <v-chart :options="stats"/>
    </div>
  </div>
</template>

<script>
import http from "@/http.js";
import ECharts from "vue-echarts";
import "echarts/lib/chart/line";
import "echarts/lib/component/tooltip";
import "echarts/lib/component/legend";

export default {
  name: "stats",
  components: {
    "v-chart": ECharts
  },
  data() {
    return {
      stats: {
        legend: {
          data: ["写入速度"],
          show: true
        },
        tooltip: {
          trigger: "axis",
          axisPointer: {
            type: "cross",
            label: {
              formatter: params => {
                if (params.seriesData.length === 0) {
                  return params.value.toFixed(2) + "MB/s";
                } else {
                  return (
                    "采样率：" +
                    this.$options.filters.sampleString(params.value)
                  );
                }
              }
            }
          }
        },
        xAxis: {
          type: "category",
          data: [],
          axisLabel: {
            formatter: this.$options.filters.sampleString
          }
        },
        yAxis: {
          type: "value",
          axisLabel: {
            formatter: "{value} MB/s"
          }
        },
        series: [
          {
            data: [],
            type: "line",
            name: "写入速度"
          }
        ]
      },
      tableData: [],
      multipleSelection: []
    };
  },
  methods: {
    handleSelectionChange(items) {
      items.sort((a, b) => {
        return a.sample - b.sample;
      });
      let samples = items.map(item => {
        return item.sample;
      });
      let speeds = items.map(item => {
        return (item.speed / (1024 * 1024)).toFixed(2);
      });
      this.stats.xAxis.data = samples;
      this.stats.series[0].data = speeds;
    }
  },
  mounted: function() {
    http.queryTaskSummary(data => {
      if (data.code !== "E_OK") {
        // eslint-disable-next-line
        console.log(data);
        return;
      }
      this.tableData = data.data;
    });
  }
};
</script>
