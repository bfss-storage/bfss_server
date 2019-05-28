import Vue from 'vue'
import Router from 'vue-router'
import Tasks from './views/Tasks.vue'

Vue.use(Router)

export default new Router({
  routes: [
    {
      path: '/tasks',
      name: 'tasks',
      component: Tasks
    },
    {
      path: '/stats',
      name: 'stats',
      // route level code-splitting
      // this generates a separate chunk (about.[hash].js) for this route
      // which is lazy-loaded when the route is visited.
      component: () => import('./views/stats.vue')
    }
  ]
})
