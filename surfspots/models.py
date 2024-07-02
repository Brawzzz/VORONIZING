from django.db import models


class Waves(models.Model):
    time = models.IntegerField()

    def __str__(self):
        return self.time
