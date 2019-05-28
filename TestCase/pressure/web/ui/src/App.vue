<template>
  <div id="app">
    <div id="nav">
      <el-menu
        :default-active="activeIndex"
        class="el-menu-demo"
        mode="horizontal"
        @select="handleSelect"
        background-color="#545c64"
        text-color="#fff"
        active-text-color="#ffd04b"
        :router="true"
      >
        <el-menu-item index="1" route="/tasks">
          <!-- <router-link class="nav-router" to="/tasks">任务中心</router-link> -->
          任务中心
        </el-menu-item>
        <el-menu-item index="2" route="stats">统计</el-menu-item>
      </el-menu>
    </div>
    <router-view/>
  </div>
</template>

<style>
#app {
  font-family: "Avenir", Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  text-align: center;
  color: #2c3e50;
}
.nav-router {
  display: inline-block;
  text-decoration: none;
}
</style>

<script>
export default {
  data() {
    return {
      activeIndex: "1"
    };
  },
  methods: {
    handleSelect(index) {
      this.activeIndex = index;
    }
  },
  watch: {
    $route(to) {
      if (to.path === "/stats") {
        this.activeIndex = "2";
      } else {
        this.activeIndex = "1";
      }
    }
  }
};
</script>
