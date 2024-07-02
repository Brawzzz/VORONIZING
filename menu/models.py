from django.db import models


class Art(models.Model):
    images = models.ImageField()
    seeds = models.IntegerField()

    def __str__(self):
        return self.images
