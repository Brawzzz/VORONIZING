# Generated by Django 3.1.6 on 2023-08-05 10:16

from django.db import migrations


class Migration(migrations.Migration):

    dependencies = [
        ('menu', '0004_art_art_id'),
    ]

    operations = [
        migrations.RemoveField(
            model_name='art',
            name='art_id',
        ),
    ]
