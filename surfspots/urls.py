from django.urls import path
from .views import index, surfspots_viewer

urlpatterns = [
    # path("", index, name="index"),
    path("", surfspots_viewer, name="surfspots_viewer"),
]


