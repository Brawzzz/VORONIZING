from django.http import HttpResponse
from django.shortcuts import render


def index(request):
    return render(request, "index.html")

def future_projects(request):
    return render(request, 'future_projects.html')

