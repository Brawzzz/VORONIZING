## ==================================================== ##
import os
import logging
import shutil
from django.conf import settings
from django.shortcuts import render, redirect, reverse
from django.http import HttpResponse
from .forms import ArtForm
from django.core.files.storage import FileSystemStorage
from pathlib import Path
import subprocess
## ==================================================== ##

logging.basicConfig(level=logging.DEBUG)

logger = logging.getLogger(__name__)
console_handler = logging.StreamHandler()

logger.addHandler(console_handler)

# BASE_DIR = Path(__file__).resolve().parent.parent

def clear_directory(directory):

    for file in os.listdir(directory):
        os.remove(os.path.join(directory, file))
    
def menu_index(request):

    clear_directory('/home/user/VORONIZING/static/voronoi_img/')

    if request.method == 'POST' and request.FILES['images']:

        form = ArtForm(request.POST, request.FILES)
        if form.is_valid():
            form.save()

            images = request.FILES['images']
            #fs = FileSystemStorage(location= str(BASE_DIR) + '/media/')
            #fs.save(images.name, images)

            seeds = form.cleaned_data['seeds']

            url = reverse("viewer", kwargs={"images": images.name, "seeds": seeds})
            return redirect(url)
    else:
        form = ArtForm()

    return render(request, 'menu-index.html', {'form': form})


def viewer(request, images, seeds):

    image_infos = images.split('.')
    images_base_name = image_infos[0]
    images_extension = image_infos[1]
    output_image_name = "voronizing_" + images_base_name + "." + images_extension

    exe_path = '/home/user/VORONIZING/menu/script/voronizing'
    input_file_arg = '/home/user/VORONIZING/media/' + images
    output_file_arg = '/home/user/VORONIZING/static/voronoi_img/' + output_image_name

    try: 
        result = subprocess.run([exe_path , str(input_file_arg), str(output_file_arg), str(seeds)] , stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        stdout = result.stdout
        stderr = result.stderr
        
        logger.debug("Sortie standard :")
        logger.debug(stdout)
        
        logger.debug("Sortie erreur :")
        logger.debug(stderr)
        
        if result.returncode != 0:
            return HttpResponse(f"Erreur lors de l'exécution du script C : {result.stderr} , {result.stdout}", status=500)
        
    except FileNotFoundError:
        logger.debug("!========== ERROR : C SCRIPT NOT FOUND ==========!")
        return HttpResponse("Le script C n'a pas été trouvé.", status=500)

    # try: 
    #     delete_file = subprocess.run(["rm" , "-f" , "/home/user/VORONIZING/media/*"] , stderr=subprocess.PIPE, text=True)
    #     stderr = delete_file.stderr

    #     if delete_file.returncode != 0 :
    #         logger.debug("!========== ERROR : DELETE COMMAND FAILED ==========!")
    #         return HttpResponse(f"ERROR : DELETE COMMAND FAILED : {result.stderr} , {result.stdout} ", status=500)
        
    # except FileNotFoundError:
    #     return HttpResponse("ERROR : COMMAND NOT FOUND", status=500)

    # subprocess.run(["/bin/rm" , "-f" , "/home/user/VORONIZING/media/*"])
    # os.remove("/home/user/VORONIZING/media/" + images)

    clear_directory('/home/user/VORONIZING/media/')
    return render(request, 'viewer.html', {'images': images, 'seeds': seeds, 'output_image': output_image_name})

    
