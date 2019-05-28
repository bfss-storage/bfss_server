package web

import (
	"pressure/app"
	"pressure/web/handler"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
)

func setupRouter() *gin.Engine {
	r := gin.Default()

	// Enable cors when in debug mode.
	if gin.Mode() == gin.DebugMode {
		config := cors.DefaultConfig()
		config.AllowMethods = []string{"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD"}
		config.AllowHeaders = []string{"Origin", "Content-Type", "Referer", "User-Agent"}
		config.AllowAllOrigins = true
		//config.AllowCredentials = true
		m := cors.New(config)
		r.Use(m)
	}
	r.Static("/static", "./webui/static")
	v1 := r.Group("/api/v1")
	{
		// new a task
		v1.POST("/tasks", handler.NewTask)
		v1.GET("/tasks", handler.QueryTask)
		v1.GET("/tasks/summary", handler.QueryTaskSummary)
		v1.POST("/tasks/start", handler.StartTask)
		v1.GET("/tasks/export", handler.ExportStats)
	}

	return r
}

// Start stats a web server
func Start() error {
	r := setupRouter()
	return r.Run(app.Cfg.WebServer)
}
