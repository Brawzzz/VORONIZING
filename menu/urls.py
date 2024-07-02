from django.urls import path
from .views import menu_index, viewer


urlpatterns = [
    path('', menu_index, name="menu-index"),
    path('viewer/<str:images>/<int:seeds>/', viewer, name="viewer"),
]