module.exports = {
    publicPath: process.env.NODE_ENV === 'production' ? '/static/' : '/',
    transpileDependencies: [
      'vue-echarts',
      'resize-detector'
    ]
  }